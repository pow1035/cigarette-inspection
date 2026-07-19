#!/usr/bin/env python3
"""Descriptive P5 comparison between model output and an unreviewed human reference.

This tool intentionally does not calculate formal model-effect metrics. Neither input
is accepted as ground truth. Its output is only for disagreement triage.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import tempfile
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


SCHEMA_VERSION = "p5-exploratory-consistency-v1"
RESULT_TYPES = (
    "spatial_match_same_class",
    "spatial_match_class_changed",
    "prediction_only",
    "human_only",
)
VALID_DECISIONS = {"OK", "NG", "REVIEW"}


class InputContractError(ValueError):
    """Raised when an input could permit a ground-truth or identity misstatement."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise InputContractError(f"JSON root must be an object: {path}")
    return value


def _atomic_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _atomic_json(path: Path, value: Any) -> None:
    _atomic_text(path, json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def _atomic_csv(path: Path, fieldnames: list[str], rows: Iterable[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fieldnames, extrasaction="raise")
            writer.writeheader()
            writer.writerows(rows)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _index_unique(items: list[dict[str, Any]], key: str, label: str) -> dict[Any, dict[str, Any]]:
    result: dict[Any, dict[str, Any]] = {}
    for item in items:
        if not isinstance(item, dict) or key not in item:
            raise InputContractError(f"{label} item missing {key}")
        item_key = item[key]
        if item_key in result:
            raise InputContractError(f"duplicate {label} {key}: {item_key}")
        result[item_key] = item
    return result


def _require_unreviewed_reference(dataset: dict[str, Any], role: str) -> None:
    info = dataset.get("info")
    if not isinstance(info, dict):
        raise InputContractError(f"{role} info must be an object")
    if info.get("ground_truth_complete") is not False:
        raise InputContractError(f"{role} must explicitly declare ground_truth_complete=false")
    if info.get("accuracy_metrics_claimed") is not False:
        raise InputContractError(f"{role} must explicitly declare accuracy_metrics_claimed=false")
    if role == "human_reference" and info.get("annotation_stage") != "pass1":
        raise InputContractError("human_reference must be an unreviewed pass1 export")

    for section in ("images", "annotations"):
        items = dataset.get(section)
        if not isinstance(items, list):
            raise InputContractError(f"{role} {section} must be a list")
        for item in items:
            if not isinstance(item, dict) or item.get("is_ground_truth") is not False:
                raise InputContractError(f"{role} {section} must explicitly keep is_ground_truth=false")


def _validate_bbox(bbox: Any, image: dict[str, Any], label: str) -> tuple[float, float, float, float]:
    if not isinstance(bbox, list) or len(bbox) != 4:
        raise InputContractError(f"{label} bbox must contain four values")
    try:
        x, y, width, height = (float(value) for value in bbox)
    except (TypeError, ValueError) as exc:
        raise InputContractError(f"{label} bbox values must be numeric") from exc
    if not all(math.isfinite(value) for value in (x, y, width, height)):
        raise InputContractError(f"{label} bbox values must be finite")
    if x < 0 or y < 0 or width <= 0 or height <= 0:
        raise InputContractError(f"{label} bbox must be positive and inside the image")
    if x + width > float(image["width"]) + 1e-6 or y + height > float(image["height"]) + 1e-6:
        raise InputContractError(f"{label} bbox exceeds image bounds")
    return x, y, width, height


def validate_inputs(predictions: dict[str, Any], human: dict[str, Any]) -> tuple[dict[int, dict[str, Any]], dict[int, dict[str, Any]], dict[int, dict[str, Any]]]:
    _require_unreviewed_reference(predictions, "predictions")
    _require_unreviewed_reference(human, "human_reference")

    pred_categories = _index_unique(predictions.get("categories", []), "id", "prediction category")
    human_categories = _index_unique(human.get("categories", []), "id", "human category")
    if pred_categories != human_categories:
        raise InputContractError("prediction and human category catalogs differ")

    pred_images = _index_unique(predictions["images"], "id", "prediction image")
    human_images = _index_unique(human["images"], "id", "human image")
    if set(pred_images) != set(human_images):
        raise InputContractError("prediction and human image ID sets differ")

    identity_fields = ("file_name", "sha256", "width", "height")
    for image_id in sorted(pred_images):
        pred_image = pred_images[image_id]
        human_image = human_images[image_id]
        for field in identity_fields:
            if pred_image.get(field) != human_image.get(field):
                raise InputContractError(f"image {image_id} identity mismatch for {field}")
        pred_decision = pred_image.get("predicted_decision")
        human_decision = human_image.get("cigarette_decision")
        if pred_decision not in VALID_DECISIONS:
            raise InputContractError(f"image {image_id} has invalid predicted_decision")
        if human_decision not in VALID_DECISIONS:
            raise InputContractError(f"image {image_id} has invalid human cigarette_decision")

    for role, dataset, images in (
        ("prediction", predictions, pred_images),
        ("human", human, human_images),
    ):
        annotations = _index_unique(dataset["annotations"], "id", f"{role} annotation")
        for annotation_id, annotation in annotations.items():
            image_id = annotation.get("image_id")
            category_id = annotation.get("category_id")
            if image_id not in images:
                raise InputContractError(f"{role} annotation {annotation_id} references an unknown image")
            if category_id not in pred_categories:
                raise InputContractError(f"{role} annotation {annotation_id} references an unknown category")
            _validate_bbox(annotation.get("bbox"), images[image_id], f"{role} annotation {annotation_id}")

    return pred_images, human_images, pred_categories


def bbox_iou(left: list[float], right: list[float]) -> float:
    lx1, ly1, lw, lh = (float(value) for value in left)
    rx1, ry1, rw, rh = (float(value) for value in right)
    lx2, ly2 = lx1 + lw, ly1 + lh
    rx2, ry2 = rx1 + rw, ry1 + rh
    intersection = max(0.0, min(lx2, rx2) - max(lx1, rx1)) * max(0.0, min(ly2, ry2) - max(ly1, ry1))
    union = lw * lh + rw * rh - intersection
    return intersection / union if union > 0 else 0.0


def match_image_boxes(pred_annotations: list[dict[str, Any]], human_annotations: list[dict[str, Any]], iou_threshold: float) -> list[dict[str, Any]]:
    candidates: list[tuple[float, int, int, dict[str, Any], dict[str, Any]]] = []
    for pred in pred_annotations:
        for human in human_annotations:
            overlap = bbox_iou(pred["bbox"], human["bbox"])
            if overlap >= iou_threshold:
                candidates.append((-overlap, int(pred["id"]), int(human["id"]), pred, human))
    candidates.sort(key=lambda item: (item[0], item[1], item[2]))

    matched_pred: set[int] = set()
    matched_human: set[int] = set()
    rows: list[dict[str, Any]] = []
    for negative_iou, pred_id, human_id, pred, human in candidates:
        if pred_id in matched_pred or human_id in matched_human:
            continue
        matched_pred.add(pred_id)
        matched_human.add(human_id)
        result_type = (
            "spatial_match_same_class"
            if pred["category_id"] == human["category_id"]
            else "spatial_match_class_changed"
        )
        rows.append({
            "result_type": result_type,
            "iou": -negative_iou,
            "prediction": pred,
            "human": human,
        })

    for pred in sorted(pred_annotations, key=lambda item: int(item["id"])):
        if int(pred["id"]) not in matched_pred:
            rows.append({"result_type": "prediction_only", "iou": None, "prediction": pred, "human": None})
    for human in sorted(human_annotations, key=lambda item: int(item["id"])):
        if int(human["id"]) not in matched_human:
            rows.append({"result_type": "human_only", "iou": None, "prediction": None, "human": human})
    return rows


def analyze(predictions: dict[str, Any], human: dict[str, Any], iou_threshold: float = 0.5) -> dict[str, Any]:
    if not math.isfinite(iou_threshold) or not 0 < iou_threshold <= 1:
        raise InputContractError("iou_threshold must be greater than 0 and at most 1")
    pred_images, human_images, categories = validate_inputs(predictions, human)

    pred_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
    human_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for annotation in predictions["annotations"]:
        pred_by_image[int(annotation["image_id"])].append(annotation)
    for annotation in human["annotations"]:
        human_by_image[int(annotation["image_id"])].append(annotation)

    image_rows: list[dict[str, Any]] = []
    box_rows: list[dict[str, Any]] = []
    review_rows: list[dict[str, Any]] = []
    decision_counts: Counter[str] = Counter()
    result_counts: Counter[str] = Counter()
    transition_counts: Counter[tuple[int, int]] = Counter()
    prediction_only_categories: Counter[int] = Counter()
    human_only_categories: Counter[int] = Counter()

    for image_id in sorted(pred_images):
        pred_image = pred_images[image_id]
        human_image = human_images[image_id]
        pred_decision = pred_image["predicted_decision"]
        human_decision = human_image["cigarette_decision"]
        if human_decision == "REVIEW":
            decision_result = "review_excluded"
        elif pred_decision == human_decision:
            decision_result = "same"
        else:
            decision_result = "different"
        decision_counts[decision_result] += 1

        matches = match_image_boxes(pred_by_image[image_id], human_by_image[image_id], iou_threshold)
        local_counts: Counter[str] = Counter(row["result_type"] for row in matches)
        result_counts.update(local_counts)
        for match_index, match in enumerate(matches, start=1):
            pred = match["prediction"]
            human_ann = match["human"]
            pred_category_id = pred["category_id"] if pred else None
            human_category_id = human_ann["category_id"] if human_ann else None
            if match["result_type"] == "spatial_match_class_changed":
                transition_counts[(int(pred_category_id), int(human_category_id))] += 1
            elif match["result_type"] == "prediction_only":
                prediction_only_categories[int(pred_category_id)] += 1
            elif match["result_type"] == "human_only":
                human_only_categories[int(human_category_id)] += 1
            box_rows.append({
                "image_id": image_id,
                "file_name": pred_image["file_name"],
                "result_index": match_index,
                "result_type": match["result_type"],
                "iou": "" if match["iou"] is None else f"{match['iou']:.6f}",
                "prediction_annotation_id": "" if pred is None else pred["id"],
                "prediction_category_id": "" if pred is None else pred_category_id,
                "prediction_category_name": "" if pred is None else categories[pred_category_id]["name"],
                "prediction_bbox": "" if pred is None else json.dumps(pred["bbox"], separators=(",", ":")),
                "human_annotation_id": "" if human_ann is None else human_ann["id"],
                "human_category_id": "" if human_ann is None else human_category_id,
                "human_category_name": "" if human_ann is None else categories[human_category_id]["name"],
                "human_bbox": "" if human_ann is None else json.dumps(human_ann["bbox"], separators=(",", ":")),
            })

        image_row = {
            "image_id": image_id,
            "file_name": pred_image["file_name"],
            "predicted_decision": pred_decision,
            "human_decision": human_decision,
            "decision_result": decision_result,
            "prediction_box_count": len(pred_by_image[image_id]),
            "human_box_count": len(human_by_image[image_id]),
            **{name: local_counts[name] for name in RESULT_TYPES},
            "human_notes": human_image.get("notes", ""),
        }
        image_rows.append(image_row)
        if human_decision == "REVIEW":
            review_rows.append(dict(image_row))

    category_name = lambda category_id: categories[category_id]["name"]
    summary = {
        "schema_version": SCHEMA_VERSION,
        "analysis_scope": "descriptive_model_vs_single_annotator_reference",
        "ground_truth_used": False,
        "formal_effect_measurement": False,
        "intended_use": "disagreement_triage_only",
        "parameters": {"iou_threshold": iou_threshold, "matching": "deterministic_spatial_first_greedy_ignore_class"},
        "image_counts": {
            "total": len(pred_images),
            "decision_same": decision_counts["same"],
            "decision_different": decision_counts["different"],
            "human_review_excluded": decision_counts["review_excluded"],
        },
        "decision_distributions": {
            "prediction": dict(sorted(Counter(image["predicted_decision"] for image in pred_images.values()).items())),
            "human_reference": dict(sorted(Counter(image["cigarette_decision"] for image in human_images.values()).items())),
        },
        "box_counts": {
            "prediction_total": len(predictions["annotations"]),
            "human_reference_total": len(human["annotations"]),
            **{name: result_counts[name] for name in RESULT_TYPES},
            "result_rows": len(box_rows),
        },
        "class_changed": [
            {"prediction_category_id": left, "prediction_category_name": category_name(left),
             "human_category_id": right, "human_category_name": category_name(right), "count": count}
            for (left, right), count in sorted(transition_counts.items())
        ],
        "prediction_only_by_category": [
            {"category_id": category_id, "category_name": category_name(category_id), "count": count}
            for category_id, count in sorted(prediction_only_categories.items())
        ],
        "human_only_by_category": [
            {"category_id": category_id, "category_name": category_name(category_id), "count": count}
            for category_id, count in sorted(human_only_categories.items())
        ],
    }
    return {"summary": summary, "image_rows": image_rows, "box_rows": box_rows, "review_rows": review_rows}


def _markdown_table(headers: list[str], rows: list[list[Any]]) -> list[str]:
    if not rows:
        return ["（无）"]
    result = ["| " + " | ".join(headers) + " |", "| " + " | ".join("---" for _ in headers) + " |"]
    result.extend("| " + " | ".join(str(value) for value in row) + " |" for row in rows)
    return result


def render_report(summary: dict[str, Any]) -> str:
    images = summary["image_counts"]
    boxes = summary["box_counts"]
    lines = [
        "# P5 模型输出与单标注员参考的探索性一致性分析",
        "",
        "> **使用限制：两份输入都不是真值。人工文件仅是未独立复核的单标注员首轮参考；本报告只用于定位分歧和安排后续检查，不用于正式效果结论、训练、阈值调整或验收。**",
        "",
        "## 方法",
        "",
        f"- 以 IoU `{summary['parameters']['iou_threshold']}` 为门槛，先忽略类别进行确定性的空间贪心匹配。",
        "- 每个框结果分为：同位置同类别、同位置类别改变、仅模型侧存在、仅人工参考侧存在。",
        "- 人工决定为 `REVIEW` 的图片单列，不进入决定一致/不一致的可比较集合。",
        "- 这里只报告数量和样本清单，不计算任何正式效果指标或比例。",
        "",
        "## 图片决定描述",
        "",
        f"- 图片总数：{images['total']}",
        f"- 决定相同：{images['decision_same']}",
        f"- 决定不同：{images['decision_different']}",
        f"- 人工 `REVIEW`，已排除：{images['human_review_excluded']}",
        "",
        "## 框级描述",
        "",
        f"- 模型框：{boxes['prediction_total']}",
        f"- 人工参考框：{boxes['human_reference_total']}",
        f"- 同位置同类别：{boxes['spatial_match_same_class']}",
        f"- 同位置类别改变：{boxes['spatial_match_class_changed']}",
        f"- 仅模型侧存在：{boxes['prediction_only']}",
        f"- 仅人工参考侧存在：{boxes['human_only']}",
        "",
        "### 同位置类别改变",
        "",
    ]
    lines.extend(_markdown_table(
        ["模型类别", "人工参考类别", "数量"],
        [[f"{item['prediction_category_id']} {item['prediction_category_name']}",
          f"{item['human_category_id']} {item['human_category_name']}", item["count"]]
         for item in summary["class_changed"]],
    ))
    lines.extend([
        "",
        "## 输出文件用法",
        "",
        "- `image-disagreements.csv`：30 张图片逐图描述。",
        "- `box-matches.csv`：所有匹配和单侧框。",
        "- `review-cases.csv`：人工决定为 `REVIEW` 的图片，供业务澄清时使用。",
        "- `summary.json`：机器可读计数。",
        "- `manifest.json`：输入与输出文件哈希绑定。",
        "",
        "> **再次提醒：模型输出不是标注真值，单标注员首轮参考也不是真值。这里的相同或不同只能作为排查线索。**",
        "",
    ])
    return "\n".join(lines)


def write_analysis(output_dir: Path, result: dict[str, Any], input_bindings: dict[str, dict[str, str]]) -> dict[str, Any]:
    output_dir.mkdir(parents=True, exist_ok=False)
    summary_path = output_dir / "summary.json"
    image_path = output_dir / "image-disagreements.csv"
    box_path = output_dir / "box-matches.csv"
    review_path = output_dir / "review-cases.csv"
    report_path = output_dir / "report.md"

    _atomic_json(summary_path, result["summary"])
    image_fields = [
        "image_id", "file_name", "predicted_decision", "human_decision", "decision_result",
        "prediction_box_count", "human_box_count", *RESULT_TYPES, "human_notes",
    ]
    _atomic_csv(image_path, image_fields, result["image_rows"])
    box_fields = [
        "image_id", "file_name", "result_index", "result_type", "iou",
        "prediction_annotation_id", "prediction_category_id", "prediction_category_name", "prediction_bbox",
        "human_annotation_id", "human_category_id", "human_category_name", "human_bbox",
    ]
    _atomic_csv(box_path, box_fields, result["box_rows"])
    _atomic_csv(review_path, image_fields, result["review_rows"])
    _atomic_text(report_path, render_report(result["summary"]))

    output_files = [summary_path, image_path, box_path, review_path, report_path]
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
        "ground_truth_used": False,
        "formal_effect_measurement": False,
        "intended_use": "disagreement_triage_only",
        "inputs": input_bindings,
        "parameters": result["summary"]["parameters"],
        "outputs": {
            path.name: {"sha256": sha256_file(path), "size_bytes": path.stat().st_size}
            for path in output_files
        },
    }
    _atomic_json(output_dir / "manifest.json", manifest)
    return manifest


def run(prediction_path: Path, human_path: Path, class_catalog_path: Path, output_dir: Path, iou_threshold: float = 0.5) -> dict[str, Any]:
    predictions = load_json(prediction_path)
    human = load_json(human_path)
    catalog = load_json(class_catalog_path)
    expected_catalog_hashes = {
        str(predictions["info"].get("class_catalog_sha256", "")).upper(),
        str(human["info"].get("class_catalog_sha256", "")).upper(),
    }
    actual_catalog_hash = sha256_file(class_catalog_path)
    if expected_catalog_hashes != {actual_catalog_hash}:
        raise InputContractError("class catalog hash does not match both input declarations")
    catalog_ids = {item.get("id") for item in catalog.get("classes", [])}
    input_ids = {item.get("id") for item in predictions.get("categories", [])}
    if catalog_ids != input_ids:
        raise InputContractError("class catalog IDs do not match the COCO categories")

    result = analyze(predictions, human, iou_threshold)
    bindings = {
        "predictions": {"path": str(prediction_path.resolve()), "sha256": sha256_file(prediction_path)},
        "human_reference": {"path": str(human_path.resolve()), "sha256": sha256_file(human_path)},
        "class_catalog": {"path": str(class_catalog_path.resolve()), "sha256": actual_catalog_hash},
    }
    manifest = write_analysis(output_dir, result, bindings)
    return {"output_dir": str(output_dir.resolve()), "summary": result["summary"], "manifest": manifest}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create a descriptive, non-ground-truth P5 disagreement analysis.")
    parser.add_argument("--predictions", required=True, type=Path)
    parser.add_argument("--human-reference", required=True, type=Path)
    parser.add_argument("--class-catalog", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--iou", type=float, default=0.5)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    result = run(args.predictions, args.human_reference, args.class_catalog, args.output_dir, args.iou)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
