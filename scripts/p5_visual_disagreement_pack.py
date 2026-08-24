#!/usr/bin/env python3
"""Build an offline Chinese visual pack for P5 disagreement triage.

Both model output and the single-annotator pass-1 reference remain non-ground-truth.
The generated pack is descriptive only and must not be used for model tuning or
formal acceptance.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import html
import json
import os
import shutil
import tempfile
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont

import p5_exploratory_consistency as consistency


SCHEMA_VERSION = "p5-exploratory-visual-pack-v1"
COLORS = {
    "spatial_match_same_class": "#18a957",
    "spatial_match_class_changed": "#ef8b17",
    "prediction_only": "#d9363e",
    "human_only": "#1677d2",
}
REASON_LABELS = {
    "decision_different": "图片决定不同",
    "class_changed": "同位置类别改变",
    "unmatched_boxes": "框位置或数量不一致",
    "human_review": "人工待确认",
}
RESULT_LABELS = {
    "spatial_match_same_class": "同位置同类别",
    "spatial_match_class_changed": "同位置类别改变",
    "prediction_only": "仅模型侧",
    "human_only": "仅人工参考侧",
}
FORBIDDEN_OUTPUT_TERMS = (
    "precision", "recall", "false_positive", "false_negative",
    "true_positive", "true_negative", "准确率", "精确率", "召回率",
)


class VisualPackError(ValueError):
    """Raised when source bindings or visual-pack constraints are violated."""


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
        raise VisualPackError(f"JSON root must be an object: {path}")
    return value


def atomic_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def atomic_json(path: Path, value: Any) -> None:
    atomic_text(path, json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def atomic_csv(path: Path, fieldnames: list[str], rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fieldnames, extrasaction="raise")
            writer.writeheader()
            writer.writerows(rows)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def atomic_image(path: Path, image: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    os.close(fd)
    try:
        image.save(temporary, format="JPEG", quality=92, subsampling=0, optimize=True)
        with open(temporary, "rb+") as stream:
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def load_font(font_path: Path | None, size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [font_path] if font_path else []
    candidates.extend([
        Path(r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
    ])
    for candidate in candidates:
        if candidate and candidate.is_file():
            try:
                return ImageFont.truetype(str(candidate), size=size)
            except (OSError, ImportError):
                continue
    try:
        return ImageFont.load_default()
    except (OSError, ImportError):
        # Pillow 10.1+ may implement load_default() through FreeType too.
        return ImageFont.load_default_imagefont()


def raster_ascii(value: Any) -> str:
    """Return deterministic ASCII text safe for Pillow's bitmap fallback font."""
    return str(value).encode("ascii", errors="backslashreplace").decode("ascii")


def resolve_bound_file(base_dir: Path, relative_name: Any, label: str) -> Path:
    """Resolve an existing bound file without allowing a directory escape."""
    if not isinstance(relative_name, str) or not relative_name:
        raise VisualPackError(f"{label} must be a non-empty relative path")
    relative = Path(relative_name)
    if relative.is_absolute() or ".." in relative.parts:
        raise VisualPackError(f"{label} escapes its allowed directory: {relative_name}")
    try:
        base = base_dir.resolve(strict=True)
        candidate = (base / relative).resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise VisualPackError(f"{label} is missing or cannot be resolved: {relative_name}") from exc
    try:
        candidate.relative_to(base)
    except ValueError as exc:
        raise VisualPackError(f"{label} escapes its allowed directory: {relative_name}") from exc
    if not candidate.is_file():
        raise VisualPackError(f"{label} is not a file: {relative_name}")
    return candidate


def validate_analysis_bindings(analysis_dir: Path, prediction_path: Path, human_path: Path, catalog_path: Path) -> dict[str, Any]:
    manifest_path = analysis_dir / "manifest.json"
    summary_path = analysis_dir / "summary.json"
    manifest = load_json(manifest_path)
    summary = load_json(summary_path)
    if manifest.get("ground_truth_used") is not False or manifest.get("formal_effect_measurement") is not False:
        raise VisualPackError("analysis manifest must remain explicitly non-ground-truth and descriptive")
    if summary.get("ground_truth_used") is not False or summary.get("formal_effect_measurement") is not False:
        raise VisualPackError("analysis summary must remain explicitly non-ground-truth and descriptive")
    if manifest.get("intended_use") != "disagreement_triage_only":
        raise VisualPackError("analysis intended use must be disagreement_triage_only")

    expected_inputs = {
        "predictions": prediction_path,
        "human_reference": human_path,
        "class_catalog": catalog_path,
    }
    for name, path in expected_inputs.items():
        binding = manifest.get("inputs", {}).get(name)
        if not isinstance(binding, dict) or str(binding.get("sha256", "")).upper() != sha256_file(path):
            raise VisualPackError(f"analysis input binding mismatch: {name}")
    for name, binding in manifest.get("outputs", {}).items():
        path = resolve_bound_file(analysis_dir, name, f"analysis output {name}")
        if not path.is_file() or sha256_file(path) != str(binding.get("sha256", "")).upper():
            raise VisualPackError(f"analysis output binding mismatch: {name}")
    return {"manifest": manifest, "summary": summary, "manifest_sha256": sha256_file(manifest_path)}


def catalog_categories(catalog: dict[str, Any]) -> dict[int, dict[str, Any]]:
    result: dict[int, dict[str, Any]] = {}
    for item in catalog.get("classes", []):
        category_id = item.get("id")
        if not isinstance(category_id, int) or isinstance(category_id, bool) or category_id in result:
            raise VisualPackError("class catalog IDs must be unique integers")
        result[category_id] = {
            "id": category_id,
            "name": item.get("name"),
            "display_name_zh": item.get("displayNameZh"),
            "mapping_status": item.get("mappingStatus"),
        }
    return result


def validate_catalog(catalog: dict[str, Any], predictions: dict[str, Any]) -> dict[int, dict[str, Any]]:
    expected = catalog_categories(catalog)
    actual = {item["id"]: item for item in predictions["categories"]}
    if set(expected) != set(actual):
        raise VisualPackError("catalog and prediction category IDs differ")
    for category_id, item in expected.items():
        source = actual[category_id]
        for catalog_key, coco_key in (
            ("name", "name"),
            ("display_name_zh", "display_name_zh"),
            ("mapping_status", "mapping_status"),
        ):
            if item[catalog_key] != source.get(coco_key):
                raise VisualPackError(f"category {category_id} differs for {coco_key}")
    return expected


def select_cases(analysis: dict[str, Any]) -> list[dict[str, Any]]:
    by_image: dict[int, dict[str, Any]] = {}
    for row in analysis["image_rows"]:
        image_id = int(row["image_id"])
        reasons: list[str] = []
        if row["decision_result"] == "different":
            reasons.append("decision_different")
        if row["decision_result"] == "review_excluded":
            reasons.append("human_review")
        by_image[image_id] = {
            "image_row": row,
            "reasons": reasons,
            "class_changed_box_count": 0,
            "prediction_only_box_count": 0,
            "human_only_box_count": 0,
        }
    for row in analysis["box_rows"]:
        image_id = int(row["image_id"])
        result_type = row["result_type"]
        if result_type == "spatial_match_class_changed":
            by_image[image_id]["class_changed_box_count"] += 1
            if "class_changed" not in by_image[image_id]["reasons"]:
                by_image[image_id]["reasons"].append("class_changed")
        elif result_type in {"prediction_only", "human_only"}:
            count_key = f"{result_type}_box_count"
            by_image[image_id][count_key] += 1
            if "unmatched_boxes" not in by_image[image_id]["reasons"]:
                by_image[image_id]["reasons"].append("unmatched_boxes")

    selected = [value for value in by_image.values() if value["reasons"]]
    priority = {
        "decision_different": 0,
        "class_changed": 1,
        "unmatched_boxes": 2,
        "human_review": 3,
    }
    selected.sort(key=lambda value: (
        min(priority[reason] for reason in value["reasons"]),
        int(value["image_row"]["image_id"]),
    ))
    return selected


def annotation_styles(matches: list[dict[str, Any]]) -> tuple[dict[int, str], dict[int, str]]:
    pred_styles: dict[int, str] = {}
    human_styles: dict[int, str] = {}
    for match in matches:
        result_type = match["result_type"]
        pred = match["prediction"]
        human = match["human"]
        if pred is not None:
            pred_styles[int(pred["id"])] = result_type
        if human is not None:
            human_styles[int(human["id"])] = result_type
    return pred_styles, human_styles


def _label_background(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, font: Any, color: str, image_width: int) -> None:
    x, y = xy
    box = draw.textbbox((x, y), text, font=font, stroke_width=0)
    width = box[2] - box[0]
    height = box[3] - box[1]
    x = max(0, min(x, image_width - width - 8))
    y = max(0, y)
    draw.rounded_rectangle((x, y, x + width + 8, y + height + 6), radius=3, fill=color)
    draw.text((x + 4, y + 2), text, font=font, fill="white")


def draw_annotations(panel: Image.Image, annotations: list[dict[str, Any]], styles: dict[int, str], categories: dict[int, dict[str, Any]], font: Any) -> None:
    draw = ImageDraw.Draw(panel)
    line_width = max(3, round(min(panel.size) / 160))
    for annotation in sorted(annotations, key=lambda item: int(item["id"])):
        result_type = styles[int(annotation["id"])]
        color = COLORS[result_type]
        x, y, width, height = (float(value) for value in annotation["bbox"])
        draw.rectangle((round(x), round(y), round(x + width), round(y + height)), outline=color, width=line_width)
        category = categories[int(annotation["category_id"])]
        label = f"{category['id']} {raster_ascii(category['name'])} - {result_type}"
        label_y = round(y) - 27
        if label_y < 0:
            label_y = round(y) + 2
        _label_background(draw, (round(x), label_y), label, font, color, panel.width)


def render_case(source_path: Path, image_info: dict[str, Any], pred_annotations: list[dict[str, Any]], human_annotations: list[dict[str, Any]], matches: list[dict[str, Any]], categories: dict[int, dict[str, Any]], reasons: list[str], font_path: Path | None, output_path: Path) -> None:
    if sha256_file(source_path) != str(image_info["sha256"]).upper():
        raise VisualPackError(f"source image hash mismatch: {source_path.name}")
    with Image.open(source_path) as source:
        source.load()
        base = source.convert("RGB")
    if base.size != (int(image_info["width"]), int(image_info["height"])):
        raise VisualPackError(f"source image dimensions mismatch: {source_path.name}")

    original = base.copy()
    model = base.copy()
    human = base.copy()
    pred_styles, human_styles = annotation_styles(matches)
    label_font = load_font(font_path, max(16, round(min(base.size) / 22)))
    draw_annotations(model, pred_annotations, pred_styles, categories, label_font)
    draw_annotations(human, human_annotations, human_styles, categories, label_font)

    header_height = max(110, round(base.height * 0.24))
    canvas = Image.new("RGB", (base.width * 3, base.height + header_height), "#f3f5f7")
    canvas.paste(original, (0, header_height))
    canvas.paste(model, (base.width, header_height))
    canvas.paste(human, (base.width * 2, header_height))
    draw = ImageDraw.Draw(canvas)
    title_font = load_font(font_path, max(24, round(base.width / 34)))
    small_font = load_font(font_path, max(18, round(base.width / 48)))
    reason_text = ", ".join(reason.replace("_", " ") for reason in reasons)
    draw.text((18, 10), f"Image {image_info['id']} - {raster_ascii(image_info['file_name'])}", font=title_font, fill="#20252b")
    draw.text((18, 52), f"Selection: {reason_text}", font=small_font, fill="#5a616a")
    panel_titles = ("Original", "Model output (not truth)", "Single-annotator reference (not truth)")
    for index, title in enumerate(panel_titles):
        draw.text((index * base.width + 18, header_height - 38), title, font=small_font, fill="#20252b")
    for x in (base.width, base.width * 2):
        draw.line((x, header_height, x, canvas.height), fill="#ffffff", width=4)
    atomic_image(output_path, canvas)


def render_html(rows: list[dict[str, Any]], summary: dict[str, Any]) -> str:
    cards = []
    for row in rows:
        badges = "".join(f'<span class="badge">{html.escape(REASON_LABELS[reason])}</span>' for reason in row["reason_codes"].split(";"))
        cards.append(f'''<article class="card">
<div class="head"><div><strong>图片 {row["image_id"]}</strong> · {html.escape(row["file_name"])}</div><div>{badges}</div></div>
<img loading="lazy" src="{html.escape(row["visual_path"])}" alt="图片 {row["image_id"]} 的模型与单标注员参考对照">
<div class="meta">模型决定：<b>{row["predicted_decision"]}</b>　单标注员决定：<b>{row["human_decision"]}</b>　模型框：{row["prediction_box_count"]}　人工参考框：{row["human_box_count"]}　类别变化框：{row["class_changed_box_count"]}　仅模型侧框：{row["prediction_only_box_count"]}　仅人工侧框：{row["human_only_box_count"]}</div>
</article>''')
    return f'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>P5 探索性分歧可视化</title>
<style>
body{{margin:0;background:#eef1f4;color:#20252b;font-family:"Microsoft YaHei",sans-serif}}main{{max-width:1500px;margin:auto;padding:28px}}h1{{margin:0 0 10px}}.warning{{background:#fff4d8;border:1px solid #e6a23c;padding:14px 18px;border-radius:8px;line-height:1.7}}.summary{{display:flex;gap:12px;flex-wrap:wrap;margin:18px 0}}.summary span,.badge{{background:#303842;color:white;border-radius:16px;padding:6px 12px}}.legend{{background:white;padding:14px 18px;border-radius:8px;margin-bottom:18px}}.legend i{{display:inline-block;width:14px;height:14px;border-radius:3px;margin:0 5px 0 14px;vertical-align:-2px}}.card{{background:white;border-radius:10px;margin:18px 0;box-shadow:0 2px 9px #0001;overflow:hidden}}.head,.meta{{padding:12px 16px;display:flex;justify-content:space-between;gap:10px;flex-wrap:wrap}}.head{{font-size:17px}}.badge{{font-size:13px;margin-left:6px;background:#59636f}}img{{display:block;width:100%;height:auto;background:#ddd}}.meta{{color:#535c66;border-top:1px solid #eee}}code{{background:#e8ebee;padding:2px 5px;border-radius:3px}}
</style></head><body><main>
<h1>P5 模型输出与单标注员参考：探索性分歧可视化</h1>
<p class="warning"><b>重要限制：</b>模型输出不是真值，单标注员首轮参考也不是真值。本页面只用于定位分歧和业务澄清，不用于训练、参数调整、正式效果结论或验收。</p>
<div class="summary"><span>重点图片 {summary['selected_image_count']}</span><span>决定不同 {summary['decision_different_image_count']}</span><span>待确认 {summary['human_review_image_count']}</span><span>类别变化框 {summary['class_changed_box_count']}</span><span>位置或数量分歧图片 {summary['unmatched_box_image_count']}</span><span>未匹配框 {summary['unmatched_box_count']}</span></div>
<div class="legend"><b>框颜色：</b><i style="background:{COLORS['spatial_match_same_class']}"></i>同位置同类别<i style="background:{COLORS['spatial_match_class_changed']}"></i>同位置类别改变<i style="background:{COLORS['prediction_only']}"></i>仅模型侧<i style="background:{COLORS['human_only']}"></i>仅人工参考侧</div>
{''.join(cards)}
<p class="warning"><b>再次提醒：</b>“相同”或“不同”只是两份非真值资料之间的描述，不能解释为正式检测表现。</p>
</main></body></html>'''


def render_readme(summary: dict[str, Any]) -> str:
    return "\n".join([
        "# P5 探索性分歧可视化包",
        "",
        "> 模型输出和单标注员首轮参考都不是真值。本包只用于定位分歧，不用于训练、参数调整、正式效果结论或验收。",
        "",
        "## 内容",
        "",
        f"- 唯一重点图片：{summary['selected_image_count']}",
        f"- 图片决定不同：{summary['decision_different_image_count']}",
        f"- 人工待确认：{summary['human_review_image_count']}",
        f"- 同位置类别改变框：{summary['class_changed_box_count']}（分布在 {summary['class_changed_image_count']} 张图片）",
        f"- 框位置或数量不一致：{summary['unmatched_box_count']} 个未匹配框（分布在 {summary['unmatched_box_image_count']} 张图片）",
        f"- 未匹配框分侧计数：模型侧 {summary['prediction_only_box_count']}，人工参考侧 {summary['human_only_box_count']}",
        "",
        "打开 `index.html` 查看中文离线画廊。每张图从左到右为原图、模型输出、单标注员参考。",
        "",
        "框颜色：绿色=同位置同类别；橙色=同位置类别改变；红色=仅模型侧；蓝色=仅人工参考侧。",
        "",
    ])


def ensure_no_forbidden_terms(paths: list[Path]) -> None:
    combined = "\n".join(path.read_text(encoding="utf-8-sig").lower() for path in paths)
    for term in FORBIDDEN_OUTPUT_TERMS:
        if term.lower() in combined:
            raise VisualPackError(f"forbidden formal-effect term found in output: {term}")


def build_pack(prediction_path: Path, human_path: Path, catalog_path: Path, analysis_dir: Path, source_images_dir: Path, output_dir: Path, font_path: Path | None = None, iou_threshold: float = 0.5) -> dict[str, Any]:
    if output_dir.exists():
        raise VisualPackError(f"output directory already exists: {output_dir}")
    binding = validate_analysis_bindings(analysis_dir, prediction_path, human_path, catalog_path)
    predictions = consistency.load_json(prediction_path)
    human = consistency.load_json(human_path)
    catalog = load_json(catalog_path)
    analysis = consistency.analyze(predictions, human, iou_threshold)
    if analysis["summary"] != binding["summary"]:
        raise VisualPackError("recomputed analysis does not match bound summary")
    categories = validate_catalog(catalog, predictions)
    selected = select_cases(analysis)
    if not selected:
        raise VisualPackError("analysis contains no disagreement cases to visualize")

    pred_images = {int(item["id"]): item for item in predictions["images"]}
    human_images = {int(item["id"]): item for item in human["images"]}
    pred_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
    human_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for annotation in predictions["annotations"]:
        pred_by_image[int(annotation["image_id"])].append(annotation)
    for annotation in human["annotations"]:
        human_by_image[int(annotation["image_id"])].append(annotation)

    # Validate every selected source and directory boundary before creating a
    # staging package. Identity failures cannot leave a visible output package.
    source_paths: dict[int, Path] = {}
    for item in selected:
        image_id = int(item["image_row"]["image_id"])
        image_info = pred_images[image_id]
        if image_info["file_name"] != human_images[image_id]["file_name"]:
            raise VisualPackError(f"image filename mismatch: {image_id}")
        source_path = resolve_bound_file(
            source_images_dir, image_info["file_name"], f"source image {image_id}")
        if sha256_file(source_path) != str(image_info["sha256"]).upper():
            raise VisualPackError(f"source image hash mismatch: {source_path.name}")
        with Image.open(source_path) as source:
            if source.size != (int(image_info["width"]), int(image_info["height"])):
                raise VisualPackError(f"source image dimensions mismatch: {source_path.name}")
        source_paths[image_id] = source_path

    output_parent = output_dir.parent
    output_parent.mkdir(parents=True, exist_ok=True)
    staging_dir = Path(tempfile.mkdtemp(
        prefix=f".{output_dir.name}.staging-", dir=output_parent))
    try:
        cases_dir = staging_dir / "cases"
        cases_dir.mkdir()
        rows: list[dict[str, Any]] = []
        source_bindings: list[dict[str, Any]] = []
        selected_ids = {int(item["image_row"]["image_id"]) for item in selected}
        matches_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
        for image_id in selected_ids:
            matches_by_image[image_id] = consistency.match_image_boxes(
                pred_by_image[image_id], human_by_image[image_id], iou_threshold)

        for item in selected:
            image_row = item["image_row"]
            image_id = int(image_row["image_id"])
            image_info = pred_images[image_id]
            source_path = source_paths[image_id]
            visual_name = f"case-{image_id:03d}.jpg"
            visual_path = cases_dir / visual_name
            render_case(
                source_path, image_info, pred_by_image[image_id], human_by_image[image_id],
                matches_by_image[image_id], categories, item["reasons"], font_path, visual_path,
            )
            rows.append({
                "priority": len(rows) + 1,
                "image_id": image_id,
                "file_name": image_info["file_name"],
                "source_sha256": sha256_file(source_path),
                "reason_codes": ";".join(item["reasons"]),
                "reason_labels_zh": "；".join(REASON_LABELS[reason] for reason in item["reasons"]),
                "predicted_decision": image_row["predicted_decision"],
                "human_decision": image_row["human_decision"],
                "prediction_box_count": int(image_row["prediction_box_count"]),
                "human_box_count": int(image_row["human_box_count"]),
                "class_changed_box_count": item["class_changed_box_count"],
                "prediction_only_box_count": item["prediction_only_box_count"],
                "human_only_box_count": item["human_only_box_count"],
                "visual_path": f"cases/{visual_name}",
            })
            source_bindings.append({
                "image_id": image_id,
                "file_name": image_info["file_name"],
                "sha256": sha256_file(source_path),
                "width": int(image_info["width"]),
                "height": int(image_info["height"]),
            })

        summary = {
            "schema_version": SCHEMA_VERSION,
            "analysis_scope": "visual_disagreement_triage_from_non_ground_truth_inputs",
            "ground_truth_used": False,
            "formal_effect_measurement": False,
            "intended_use": "disagreement_triage_only",
            "selected_image_count": len(rows),
            "decision_different_image_count": sum("decision_different" in row["reason_codes"].split(";") for row in rows),
            "human_review_image_count": sum("human_review" in row["reason_codes"].split(";") for row in rows),
            "class_changed_image_count": sum("class_changed" in row["reason_codes"].split(";") for row in rows),
            "class_changed_box_count": sum(row["class_changed_box_count"] for row in rows),
            "unmatched_box_image_count": sum("unmatched_boxes" in row["reason_codes"].split(";") for row in rows),
            "prediction_only_box_count": sum(row["prediction_only_box_count"] for row in rows),
            "human_only_box_count": sum(row["human_only_box_count"] for row in rows),
            "unmatched_box_count": sum(
                row["prediction_only_box_count"] + row["human_only_box_count"]
                for row in rows
            ),
            "case_order": "decision_different_then_class_changed_then_unmatched_boxes_then_human_review",
            "iou_threshold": iou_threshold,
            "color_legend": COLORS,
        }
        summary_path = staging_dir / "summary.json"
        index_csv_path = staging_dir / "case-index.csv"
        html_path = staging_dir / "index.html"
        readme_path = staging_dir / "README.md"
        atomic_json(summary_path, summary)
        atomic_csv(index_csv_path, list(rows[0]), rows)
        atomic_text(html_path, render_html(rows, summary))
        atomic_text(readme_path, render_readme(summary))
        ensure_no_forbidden_terms([summary_path, index_csv_path, html_path, readme_path])

        output_paths = [
            summary_path, index_csv_path, html_path, readme_path,
            *sorted(cases_dir.glob("*.jpg")),
        ]
        manifest = {
            "schema_version": SCHEMA_VERSION,
            "ground_truth_used": False,
            "formal_effect_measurement": False,
            "intended_use": "disagreement_triage_only",
            "inputs": {
                "predictions": {"path": str(prediction_path.resolve()), "sha256": sha256_file(prediction_path)},
                "human_reference": {"path": str(human_path.resolve()), "sha256": sha256_file(human_path)},
                "class_catalog": {"path": str(catalog_path.resolve()), "sha256": sha256_file(catalog_path)},
                "analysis_manifest": {"path": str((analysis_dir / "manifest.json").resolve()), "sha256": binding["manifest_sha256"]},
            },
            "source_images": source_bindings,
            "parameters": {
                "iou_threshold": iou_threshold,
                "font_path": "auto" if font_path is None else str(font_path.resolve()),
            },
            "outputs": {
                str(path.relative_to(staging_dir)).replace("\\", "/"): {
                    "sha256": sha256_file(path), "size_bytes": path.stat().st_size,
                }
                for path in output_paths
            },
        }
        atomic_json(staging_dir / "manifest.json", manifest)
        os.replace(staging_dir, output_dir)
    except Exception:
        if staging_dir.exists():
            resolved_staging = staging_dir.resolve()
            resolved_parent = output_parent.resolve()
            try:
                resolved_staging.relative_to(resolved_parent)
            except ValueError as exc:
                raise RuntimeError("refusing to clean staging outside output parent") from exc
            if resolved_staging == resolved_parent:
                raise RuntimeError("refusing to clean output parent as staging")
            shutil.rmtree(resolved_staging)
        raise
    return {"output_dir": str(output_dir.resolve()), "summary": summary, "manifest": manifest}

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build an offline Chinese visual disagreement pack for P5.")
    parser.add_argument("--predictions", required=True, type=Path)
    parser.add_argument("--human-reference", required=True, type=Path)
    parser.add_argument("--class-catalog", required=True, type=Path)
    parser.add_argument("--analysis-dir", required=True, type=Path)
    parser.add_argument("--source-images", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--font", type=Path)
    parser.add_argument("--iou", type=float, default=0.5)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    result = build_pack(
        args.predictions, args.human_reference, args.class_catalog, args.analysis_dir,
        args.source_images, args.output_dir, args.font, args.iou,
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
