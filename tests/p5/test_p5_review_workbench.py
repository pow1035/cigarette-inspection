import copy
import csv
import hashlib
import http.client
import importlib.util
import json
import os
import subprocess
import struct
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import zlib
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "p5_review_workbench", ROOT / "tools" / "p5_review_workbench" / "server.py")
SERVER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(SERVER)


def png_bytes(width, height, value):
    def chunk(kind, data):
        checksum = zlib.crc32(kind + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)

    rows = b"".join(b"\x00" + bytes([value]) * width for _ in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def make_package(root):
    package = root / "review-package"
    images_dir = package / "images"
    previews_dir = package / "previews"
    static_dir = root / "static"
    images_dir.mkdir(parents=True)
    previews_dir.mkdir()
    static_dir.mkdir()
    (static_dir / "index.html").write_text("<!doctype html><title>P5</title>", encoding="utf-8")

    images = []
    rows = []
    preview_records = []
    for index in range(30):
        image_id = index + 1
        name = f"sample-{image_id:02d}.png"
        width, height = 100 + index, 50 + index
        payload = png_bytes(width, height, index)
        (images_dir / name).write_bytes(payload)
        (previews_dir / f"sample-{image_id:02d}.annotated.png").write_bytes(payload)
        sha = hashlib.sha256(payload).hexdigest()
        preview_records.append({
            "file_name": name, "source_image_sha256": sha,
            "preview_sha256": sha, "detector_version": "fixture-prediction",
            "defect_count": 1 if image_id == 1 else 0,
        })
        images.append({
            "id": image_id, "file_name": name, "width": width, "height": height,
            "sha256": sha, "source_group": "fixture", "split": "pilot",
            "annotation_status": "preannotated", "is_ground_truth": False,
            "authorization_status": "unverified",
            "cigarette_decision": "NG" if image_id == 1 else "OK",
            "predicted_decision": "NG" if image_id == 1 else "OK",
            "p4_result_present": True,
        })
        rows.append({
            "file_name": name, "sha256": sha, "source_group": "fixture",
            "dimensions": f"{width}x{height}",
            "predicted_decision": "NG" if image_id == 1 else "OK",
            "predicted_class_ids": "1" if image_id == 1 else "",
            "predicted_box_count": "1" if image_id == 1 else "0",
            "requires_business_mapping_review": "false", "selection_reasons": "fixture",
            "human_decision": "", "human_class_ids": "", "annotator": "",
            "reviewer": "", "review_status": "pending", "notes": "",
        })
    catalog_path = ROOT / "config" / "p5-class-catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    categories = [{
        "id": item["id"], "name": item["name"],
        "display_name_zh": item["displayNameZh"],
        "mapping_status": item["mappingStatus"],
        "supercategory": "cigarette_defect",
    } for item in catalog["classes"]]
    annotations = [{
        "id": 1, "image_id": 1, "category_id": 1, "bbox": [1, 2, 10, 5],
        "area": 50, "iscrowd": 0, "score": 0.75,
        "detector_version": "fixture-prediction", "source": "prediction",
        "annotation_status": "preannotated", "is_ground_truth": False,
    }]
    coco = {
        "info": {
            "ground_truth_complete": False, "accuracy_metrics_claimed": False,
            "class_catalog_sha256": hashlib.sha256(catalog_path.read_bytes()).hexdigest(),
        },
        "images": images, "annotations": annotations, "categories": categories,
    }
    (package / "pilot-preannotations.coco.json").write_text(
        json.dumps(coco), encoding="utf-8")
    with (package / "pilot-review.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (package / "pilot-manifest.json").write_text(json.dumps({"images": images}), encoding="utf-8")
    (package / "pilot-selection.json").write_text(json.dumps({"count": 30}), encoding="utf-8")
    (package / "pilot-preview-provenance.json").write_text(json.dumps({
        "valid": True, "pilot_image_count": 30, "verified_record_count": 30,
        "records": preview_records,
    }), encoding="utf-8")
    return package, static_dir


def make_development_package(root, train_count=3, validation_count=2):
    package = root / "development-package"
    static_dir = root / "development-static"
    package.mkdir(parents=True)
    static_dir.mkdir()
    (static_dir / "index.html").write_text(
        "<!doctype html><title>P5 development</title>", encoding="utf-8")

    catalog_path = ROOT / "config" / "p5-class-catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    categories = [{
        "id": item["id"], "name": item["name"],
        "display_name_zh": item["displayNameZh"],
        "mapping_status": item["mappingStatus"],
        "supercategory": "cigarette_defect",
    } for item in catalog["classes"]]
    catalog_hash = hashlib.sha256(catalog_path.read_bytes()).hexdigest()

    manifest_images = []
    review_rows = []
    split_payloads = {}
    next_id = 1
    next_annotation_id = 1
    for split, count in (("train", train_count), ("validation", validation_count)):
        images_dir = package / split / "images"
        images_dir.mkdir(parents=True)
        images = []
        annotations = []
        for index in range(count):
            image_id = next_id
            next_id += 1
            name = f"{split}-{index + 1:02d}.png"
            width, height = 80 + image_id, 40 + image_id
            payload = png_bytes(width, height, image_id)
            (images_dir / name).write_bytes(payload)
            sha = hashlib.sha256(payload).hexdigest()
            decision = "NG" if index == 0 else "OK"
            image = {
                "id": image_id, "file_name": name, "width": width,
                "height": height, "sha256": sha, "source_group": "fixture",
                "split": split, "annotation_status": "preannotated",
                "is_ground_truth": False, "authorization_status": "unverified",
                "cigarette_decision": decision, "p4_result_present": True,
            }
            images.append(image)
            manifest_images.append({
                **image, "relative_path": name, "format": "PNG",
                "canonical": True, "canonical_file_name": name,
            })
            review_rows.append({
                "split": split, "file_name": name, "sha256": sha,
                "source_group": "fixture", "dimensions": f"{width}x{height}",
                "predicted_decision": decision,
                "human_review_status": "pending", "reviewed_decision": "",
                "reviewer_notes": "",
            })
            if decision == "NG":
                annotations.append({
                    "id": next_annotation_id, "image_id": image_id,
                    "category_id": 1, "bbox": [1, 2, 10, 5], "area": 50,
                    "iscrowd": 0, "score": 0.75,
                    "detector_version": "fixture-prediction",
                    "source": "prediction", "annotation_status": "preannotated",
                    "is_ground_truth": False,
                })
                next_annotation_id += 1
        split_payloads[split] = {
            "info": {
                "ground_truth_complete": False,
                "accuracy_metrics_claimed": False,
                "class_catalog_sha256": catalog_hash,
                "human_review_status": "pending",
                "training_complete": False,
                "split": split,
            },
            "images": images, "annotations": annotations,
            "categories": categories,
        }
        (package / f"{split}-preannotations.coco.json").write_text(
            json.dumps(split_payloads[split]), encoding="utf-8")

    for index in range(2):
        name = f"pilot-{index + 1:02d}.png"
        payload = png_bytes(70 + index, 35 + index, 90 + index)
        sha = hashlib.sha256(payload).hexdigest()
        manifest_images.append({
            "id": next_id, "file_name": name, "width": 70 + index,
            "height": 35 + index, "sha256": sha, "source_group": "fixture",
            "split": "pilot", "annotation_status": "preannotated",
            "is_ground_truth": False, "authorization_status": "unverified",
            "p4_result_present": True, "relative_path": name, "format": "PNG",
            "canonical": True, "canonical_file_name": name,
        })
        next_id += 1

    with (package / "review.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(review_rows[0]))
        writer.writeheader()
        writer.writerows(review_rows)
    development_manifest = {
        "schema_version": "p5-dataset-manifest-v1",
        "images": manifest_images,
        "development_split": {
            "schema_version": "p5-development-split-v1",
            "canonical_counts": {
                "pilot": 2, "train": train_count,
                "validation": validation_count,
                "excluded_near_pilot_unassigned": 0,
            },
        },
    }
    (package / "development-manifest.json").write_text(
        json.dumps(development_manifest), encoding="utf-8")
    (package / "selection.json").write_text(json.dumps({
        "schema_version": "p5-development-selection-v1",
        "train_count": train_count, "validation_count": validation_count,
        "ground_truth": False, "accuracy_metrics_claimed": False,
        "human_review_status": "pending", "training_complete": False,
    }), encoding="utf-8")
    write_development_evidence_manifest(package)
    return package, static_dir


def write_development_evidence_manifest(package, **overrides):
    outputs = []
    for path in sorted(
            (item for item in package.rglob("*")
             if item.is_file() and item.name != "manifest.json"),
            key=lambda item: item.relative_to(package).as_posix()):
        outputs.append({
            "path": path.relative_to(package).as_posix(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "size": path.stat().st_size,
        })
    manifest = {
        "schema_version": "p5-development-package-evidence-v1",
        "status": "PASS",
        "outputs": outputs,
        "ground_truth": False,
        "accuracy_metrics_claimed": False,
        "human_review_status": "pending",
        "training_complete": False,
    }
    manifest.update(overrides)
    (package / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")


class WorkbenchServerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        root = Path(self.temp.name)
        package, static_dir = make_package(root)
        self.workspace = root / "workspace"
        self.server = SERVER.create_server(package, self.workspace, port=0, static_dir=static_dir)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        self.temp.cleanup()

    def request(self, method, path, payload=None, raw=None, headers=None):
        if raw is None and payload is not None:
            raw = json.dumps(payload).encode("utf-8")
        request_headers = {"Content-Type": "application/json"}
        request_headers.update(headers or {})
        request = urllib.request.Request(
            self.base + path, data=raw, headers=request_headers, method=method)
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                body = response.read()
                return response.status, json.loads(body) if body else None
        except urllib.error.HTTPError as error:
            body = error.read()
            return error.code, json.loads(body) if body else None

    def state(self):
        return self.request("GET", "/api/state")[1]

    @staticmethod
    def complete(state, operator="annotator-a"):
        state["operator_id"] = operator
        for image in state["images"]:
            image["review_state"] = "annotation-complete"
        return state

    def test_health_state_save_and_static_image_success(self):
        status, health = self.request("GET", "/api/health")
        self.assertEqual((status, health["status"]), (200, "ok"))
        request = urllib.request.Request(self.base + "/favicon.ico", method="GET")
        with urllib.request.urlopen(request, timeout=5) as response:
            self.assertEqual(response.status, 204)
            self.assertEqual(response.read(), b"")
        state = self.state()
        self.assertEqual(len(state["images"]), 30)
        state["operator_id"] = "annotator-a"
        state["images"][1]["notes"] = "checked"
        status, saved = self.request("POST", "/api/save", state)
        self.assertEqual(status, 200)
        self.assertEqual(saved["revision"], 1)
        self.assertTrue((self.workspace / "review-state.json").is_file())
        with urllib.request.urlopen(self.base + "/images/sample-01.png") as response:
            self.assertEqual(response.status, 200)
            self.assertEqual(response.headers.get_content_type(), "image/png")

    def test_frontend_delta_envelope_preserves_identity_and_assigns_numeric_box_id(self):
        state = self.state()
        editable = []
        for image in state["images"]:
            editable.append({
                "id": image["id"], "decision": image["decision"],
                "review_state": image["review_state"], "notes": image["notes"],
                "boxes": copy.deepcopy(image["boxes"]),
            })
        editable[1]["decision"] = "NG"
        editable[1]["review_state"] = "complete"
        editable[1]["boxes"] = [{
            "id": "client-box-token", "category_id": 2, "bbox": [2, 3, 8, 9],
            "source": "human",
        }]
        status, saved = self.request("POST", "/api/save", {
            "revision": state["session"]["revision"],
            "operator_id": "annotator-a",
            "images": editable,
        })
        self.assertEqual(status, 200)
        self.assertEqual(saved["session"]["revision"], 1)
        self.assertEqual(saved["images"][1]["review_state"], "complete")
        self.assertIs(type(saved["images"][1]["boxes"][0]["id"]), int)
        self.assertEqual(saved["images"][1]["sha256"], state["images"][1]["sha256"])

    def test_traversal_is_rejected(self):
        status, payload = self.request("GET", "/images/%2e%2e/pilot-review.csv")
        self.assertEqual(status, 400)
        self.assertIn("path", payload["error"])
        status, _ = self.request("GET", "/static/%2e%2e/review-package/pilot-review.csv")
        self.assertEqual(status, 400)

    def test_stale_revision_does_not_mutate_state(self):
        stale = self.state()
        current = copy.deepcopy(stale)
        current["operator_id"] = "first"
        self.assertEqual(self.request("POST", "/api/save", current)[0], 200)
        stale["operator_id"] = "stale"
        status, payload = self.request("POST", "/api/save", stale)
        self.assertEqual(status, 409)
        self.assertIn("stale", payload["error"])
        self.assertEqual(self.state()["operator_id"], "first")

    def test_invalid_bbox_and_bool_ids_are_rejected(self):
        invalid = self.state()
        invalid["images"][0]["boxes"][0]["bbox"] = [95, 0, 10, 5]
        status, _ = self.request("POST", "/api/save", invalid)
        self.assertEqual(status, 400)
        invalid = self.state()
        invalid["images"][0]["id"] = True
        status, payload = self.request("POST", "/api/save", invalid)
        self.assertEqual(status, 400)
        self.assertIn("integer", payload["error"])
        invalid = self.state()
        invalid["images"][0]["boxes"][0]["category_id"] = True
        self.assertEqual(self.request("POST", "/api/save", invalid)[0], 400)

    def test_categories_are_bound_for_package_and_persisted_state(self):
        invalid = self.state()
        invalid["categories"][0]["name"] = "forged-class"
        status, payload = self.request("POST", "/api/save", invalid)
        self.assertEqual(status, 400)
        self.assertIn("categories", payload["error"])

        state_path = self.workspace / "review-state.json"
        persisted = json.loads(state_path.read_text(encoding="utf-8"))
        persisted["categories"] = persisted["categories"][:1]
        state_path.write_text(json.dumps(persisted), encoding="utf-8")
        with self.assertRaisesRegex(SERVER.WorkbenchError, "categories"):
            SERVER.ReviewWorkbench(
                self.server.workbench.package, self.workspace,
                self.server.workbench.static_dir)

        with tempfile.TemporaryDirectory() as temp:
            package, static_dir = make_package(Path(temp))
            coco_path = package / "pilot-preannotations.coco.json"
            coco = json.loads(coco_path.read_text(encoding="utf-8"))
            coco["categories"].append(copy.deepcopy(coco["categories"][0]))
            coco_path.write_text(json.dumps(coco), encoding="utf-8")
            with self.assertRaisesRegex(SERVER.WorkbenchError, "categories"):
                SERVER.ReviewWorkbench(package, Path(temp) / "workspace", static_dir)

    def test_incomplete_pass1_is_refused(self):
        state = self.state()
        state["operator_id"] = "annotator-a"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        status, payload = self.request("POST", "/api/export-pass1", {})
        self.assertEqual(status, 409)
        self.assertIn("annotation-complete", payload["error"])
        self.assertFalse((self.workspace / "pass1-annotations.coco.json").exists())

    def test_invalid_completed_decision_box_combinations_are_rejected(self):
        state = self.state()
        state["images"][0]["decision"] = "REVIEW"
        state["images"][0]["review_state"] = "annotation-complete"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 400)
        state = self.state()
        state["images"][1]["decision"] = "NG"
        state["images"][1]["review_state"] = "annotation-complete"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 400)
        state = self.state()
        state["images"][0]["boxes"][0]["category_id"] = 7
        state["images"][0]["review_state"] = "annotation-complete"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 400)
        state = self.state()
        state["images"][0]["boxes"] = []
        state["images"][0]["decision"] = "REVIEW"
        state["images"][0]["notes"] = ""
        state["images"][0]["review_state"] = "annotation-complete"
        state["operator_id"] = "annotator-a"
        status, payload = self.request("POST", "/api/save", state)
        self.assertEqual(status, 400)
        self.assertIn("requires notes", payload["error"])

    def test_non_loopback_bind_is_refused(self):
        with self.assertRaisesRegex(SERVER.WorkbenchError, "loopback"):
            SERVER.create_server(
                self.server.workbench.package, self.workspace, host="0.0.0.0", port=0)

    def test_non_local_host_header_is_refused(self):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_address[1])
        connection.request("GET", "/api/health", headers={"Host": "attacker.example"})
        response = connection.getresponse()
        self.assertEqual(response.status, 400)
        self.assertIn("localhost", response.read().decode("utf-8"))
        connection.close()
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_address[1])
        connection.request("GET", "/api/health", headers={"Host": "["})
        response = connection.getresponse()
        self.assertEqual(response.status, 400)
        self.assertIn("malformed", response.read().decode("utf-8"))
        connection.close()

    def test_runtime_source_and_preview_replacement_is_refused(self):
        source_path = self.server.workbench.images_dir / "sample-01.png"
        preview_path = self.server.workbench.previews_dir / "sample-01.annotated.png"
        replacement = png_bytes(100, 50, 99)
        source_path.write_bytes(replacement)
        preview_path.write_bytes(replacement)
        status, payload = self.request("GET", "/images/sample-01.png")
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])
        status, payload = self.request("GET", "/previews/sample-01.annotated.png")
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])
        status, payload = self.request("POST", "/api/save", self.state())
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])
        status, payload = self.request("POST", "/api/export-pass1")
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])
        self.assertFalse((self.workspace / "pass1-annotations.coco.json").exists())

    def test_workspace_and_previews_are_bound_to_package(self):
        state = self.state()
        state["operator_id"] = "annotator-a"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        selection_path = self.server.workbench.package / "pilot-selection.json"
        selection_path.write_text(json.dumps({"count": 29}), encoding="utf-8")
        with self.assertRaisesRegex(SERVER.WorkbenchError, "selected package"):
            SERVER.ReviewWorkbench(
                self.server.workbench.package, self.workspace,
                self.server.workbench.static_dir)

        with tempfile.TemporaryDirectory() as temp:
            package, static_dir = make_package(Path(temp))
            (package / "previews" / "sample-01.annotated.png").write_bytes(b"not an image")
            with self.assertRaisesRegex(SERVER.WorkbenchError, "preview provenance"):
                SERVER.ReviewWorkbench(package, Path(temp) / "workspace", static_dir)

    def test_legacy_pilot_workspace_fingerprint_is_migrated(self):
        state_path = self.server.workbench.state_path
        state = json.loads(state_path.read_text(encoding="utf-8"))
        legacy_fingerprint = hashlib.sha256(json.dumps(
            self.server.workbench.package_bindings,
            sort_keys=True, separators=(",", ":")
        ).encode("utf-8")).hexdigest()
        state["package_fingerprint"] = legacy_fingerprint
        state.pop("package_kind")
        state.pop("dataset_split")
        state_path.write_text(json.dumps(state), encoding="utf-8")

        reopened = SERVER.ReviewWorkbench(
            self.server.workbench.package, self.workspace,
            self.server.workbench.static_dir)
        migrated = json.loads(state_path.read_text(encoding="utf-8"))
        self.assertEqual(migrated["package_fingerprint"], reopened.package_fingerprint)
        self.assertEqual(migrated["package_kind"], "pilot")
        self.assertEqual(migrated["dataset_split"], "pilot")
        self.assertEqual(reopened.get_state()["revision"], state["revision"])

    def test_legacy_state_schema_is_not_rewritten_without_audited_migration(self):
        state_path = self.server.workbench.state_path
        state = json.loads(state_path.read_text(encoding="utf-8"))
        state["state_schema_version"] = "p5-workbench-state-v1"
        for image in state["images"]:
            image.pop("annotator_id")
        state_path.write_text(json.dumps(state), encoding="utf-8")
        before = state_path.read_bytes()

        with self.assertRaisesRegex(
                SERVER.WorkbenchError, "explicit audited migration"):
            SERVER.ReviewWorkbench(
                self.server.workbench.package, self.workspace,
                self.server.workbench.static_dir)
        self.assertEqual(state_path.read_bytes(), before)

    def test_pass1_export_has_human_provenance_and_no_prediction_fields(self):
        self.server.workbench.source_coco["info"].update({
            "reviewed_by": "forged-reviewer", "authorization_status": "approved"})
        self.server.workbench.source_coco["images"][0].update({
            "reviewed_by": "forged-reviewer", "review_authorization": "approved",
            "ground_truth_authorized": True, "authorization_status": "approved"})
        state = self.complete(self.state())
        status, saved = self.request("POST", "/api/save", state)
        self.assertEqual(status, 200)
        self.assertEqual(saved["revision"], 1)
        status, result = self.request("POST", "/api/export-pass1")
        self.assertEqual(status, 200)
        self.assertFalse(result["is_ground_truth"])
        exported = json.loads((self.workspace / "pass1-annotations.coco.json").read_text(encoding="utf-8"))
        self.assertEqual(len(exported["images"]), 30)
        self.assertEqual(len(exported["annotations"]), 1)
        self.assertTrue(all(image["annotation_status"] == "annotated" for image in exported["images"]))
        self.assertTrue(all(image["source"] == "human" and image["is_ground_truth"] is False
                            for image in exported["images"]))
        annotation = exported["annotations"][0]
        self.assertEqual(annotation["source"], "human")
        self.assertFalse(annotation["is_ground_truth"])
        self.assertNotIn("score", annotation)
        self.assertNotIn("detector_version", annotation)
        self.assertNotIn("predicted_decision", exported["images"][0])
        self.assertNotIn("reviewed_by", exported["info"])
        self.assertNotIn("authorization_status", exported["info"])
        self.assertNotIn("reviewed_by", exported["images"][0])
        self.assertNotIn("review_authorization", exported["images"][0])
        self.assertNotIn("ground_truth_authorized", exported["images"][0])
        self.assertEqual(exported["images"][0]["authorization_status"], "unverified")
        self.assertEqual(exported["categories"], self.server.workbench.categories)
        self.assertEqual(exported["info"]["state_revision"], 1)
        self.assertEqual(exported["info"]["package_fingerprint"], result["package_fingerprint"])
        self.assertEqual(exported["info"]["review_state_sha256"], result["review_state_sha256"])
        self.assertEqual(exported["images"][0]["annotated_by"], "annotator-a")
        self.assertEqual(exported["images"][0]["completed_revision"], 1)

    def test_shared_workspace_stale_writer_and_disk_tamper_are_rejected(self):
        second = SERVER.ReviewWorkbench(
            self.server.workbench.package, self.workspace,
            self.server.workbench.static_dir)
        first_state = self.request("GET", "/api/state")[1]
        second_state = second.get_state()
        first_state["operator_id"] = "writer-a"
        self.assertEqual(self.request("POST", "/api/save", first_state)[0], 200)
        second_state["operator_id"] = "writer-b"
        with self.assertRaisesRegex(SERVER.WorkbenchError, "stale revision"):
            second.save(second_state)

        complete = self.request("GET", "/api/state")[1]
        complete["operator_id"] = "writer-a"
        for image in complete["images"]:
            image["review_state"] = "complete"
            image["decision"] = "OK"
            image["boxes"] = []
        self.assertEqual(self.request("POST", "/api/save", complete)[0], 200)
        self.server.workbench.state_path.write_text("{invalid", encoding="utf-8")
        status, payload = self.request("POST", "/api/export-pass1", {})
        self.assertEqual(status, 409)
        self.assertIn("workspace state is unavailable or invalid", payload["error"])

    def test_completed_images_cannot_be_resigned_by_global_operator_change(self):
        state = self.complete(self.state(), "alice")
        status, saved = self.request("POST", "/api/save", state)
        self.assertEqual(status, 200)
        self.assertTrue(all(item["annotated_by"] == "alice" for item in saved["images"]))
        saved["operator_id"] = "bob"
        status, saved_again = self.request("POST", "/api/save", saved)
        self.assertEqual(status, 200)
        self.assertTrue(all(item["annotated_by"] == "alice" for item in saved_again["images"]))
        self.assertEqual(self.request("POST", "/api/export-pass1")[0], 200)
        exported = json.loads((self.workspace / "pass1-annotations.coco.json").read_text(encoding="utf-8"))
        self.assertEqual(exported["info"]["annotators"], ["alice"])
        self.assertTrue(all(item["annotated_by"] == "alice" for item in exported["images"]))

    def test_mixed_confirmed_and_unconfirmed_boxes_can_complete_as_review(self):
        state = self.complete(self.state())
        image = state["images"][0]
        confirmed_category_id = image["boxes"][0]["category_id"]
        image["decision"] = "REVIEW"
        image["notes"] = "mixed confirmed and pending regions require review"
        next_box_id = max(box["id"] for item in state["images"] for box in item["boxes"]) + 1
        image["boxes"].append({
            "id": next_box_id, "category_id": 7, "bbox": [30, 30, 8, 8], "source": "human"})
        status, saved = self.request("POST", "/api/save", state)
        self.assertEqual(status, 200)
        self.assertEqual(saved["images"][0]["decision"], "REVIEW")
        self.assertEqual(len(saved["images"][0]["boxes"]), 2)
        self.assertEqual(self.request("POST", "/api/export-pass1")[0], 200)
        exported = json.loads((self.workspace / "pass1-annotations.coco.json").read_text(encoding="utf-8"))
        mixed = [item for item in exported["annotations"] if item["image_id"] == image["id"]]
        self.assertEqual({item["category_id"] for item in mixed}, {confirmed_category_id, 7})
        self.assertFalse(exported["images"][0]["is_ground_truth"])

    def test_unconfirmed_ng_is_normalized_to_review_before_save(self):
        state = self.complete(self.state())
        state["images"][0]["boxes"][0]["category_id"] = 7
        state["images"][0]["notes"] = "class requires business review"
        status, saved = self.request("POST", "/api/save", state)
        self.assertEqual(status, 200)
        self.assertEqual(saved["images"][0]["decision"], "REVIEW")
        self.assertEqual(self.request("POST", "/api/export-pass1")[0], 200)

    def test_reviewed_export_is_controlled_refusal(self):
        status, payload = self.request("POST", "/api/export-reviewed")
        self.assertEqual(status, 409)
        self.assertEqual(payload["error"], "authorization and independent reviewer required")

    def test_malformed_and_oversized_json_are_rejected(self):
        status, payload = self.request("POST", "/api/save", raw=b"{")
        self.assertEqual(status, 400)
        self.assertIn("malformed", payload["error"])
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_address[1], timeout=5)
        connection.request(
            "POST", "/api/save", body=b" " * (SERVER.MAX_JSON_BYTES + 1),
            headers={"Content-Type": "application/json"})
        response = connection.getresponse()
        self.assertEqual(response.status, 413)
        response.read()
        connection.close()


class DevelopmentWorkbenchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.package, self.static_dir = make_development_package(self.root)
        self.workspace = self.root / "train-workspace"
        self.server = SERVER.create_server(
            self.package, self.workspace, port=0,
            static_dir=self.static_dir, split="train")
        self.thread = threading.Thread(
            target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        self.temp.cleanup()

    def request(self, method, path, payload=None):
        raw = json.dumps(payload).encode("utf-8") if payload is not None else None
        request = urllib.request.Request(
            self.base + path, data=raw,
            headers={"Content-Type": "application/json"}, method=method)
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                body = response.read()
                return response.status, json.loads(body) if body else None
        except urllib.error.HTTPError as error:
            body = error.read()
            return error.code, json.loads(body) if body else None

    def test_train_state_health_and_no_rendered_preview(self):
        status, health = self.request("GET", "/api/health")
        self.assertEqual(status, 200)
        self.assertEqual(health["package_kind"], "development")
        self.assertEqual(health["dataset_split"], "train")
        self.assertEqual(health["image_count"], 3)
        status, state = self.request("GET", "/api/state")
        self.assertEqual(status, 200)
        self.assertEqual(state["session"]["dataset_split"], "train")
        self.assertEqual(len(state["images"]), 3)
        self.assertTrue(all(image["preview_url"] is None for image in state["images"]))
        status, payload = self.request(
            "GET", "/previews/train-01.annotated.png")
        self.assertEqual(status, 404)
        self.assertIn("does not include", payload["error"])

    def test_train_pass1_export_preserves_split_and_non_truth_status(self):
        state = self.request("GET", "/api/state")[1]
        state["operator_id"] = "train-annotator"
        for image in state["images"]:
            image["review_state"] = "annotation-complete"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        status, result = self.request("POST", "/api/export-pass1", {})
        self.assertEqual(status, 200)
        self.assertEqual(result["dataset_split"], "train")
        export_path = self.workspace / "pass1-train-annotations.coco.json"
        self.assertEqual(Path(result["path"]), export_path)
        exported = json.loads(export_path.read_text(encoding="utf-8"))
        self.assertEqual(exported["info"]["dataset_split"], "train")
        self.assertEqual(exported["info"]["package_kind"], "development")
        self.assertFalse(exported["info"]["ground_truth_complete"])
        self.assertTrue(all(image["split"] == "train" for image in exported["images"]))
        self.assertTrue(all(image["is_ground_truth"] is False
                            for image in exported["images"]))

    def test_workspace_cannot_be_reused_for_validation(self):
        self.assertTrue((self.workspace / "review-state.json").is_file())
        with self.assertRaisesRegex(
                SERVER.WorkbenchError, "selected package|dataset split"):
            SERVER.ReviewWorkbench(
                self.package, self.workspace, self.static_dir, "validation")

    def test_runtime_development_source_replacement_is_refused(self):
        state = self.request("GET", "/api/state")[1]
        source_path = self.server.workbench.images_dir / "train-01.png"
        source_path.write_bytes(png_bytes(81, 41, 99))
        status, payload = self.request("GET", "/images/train-01.png")
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])
        status, payload = self.request("GET", "/api/state")
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])
        status, payload = self.request("POST", "/api/save", state)
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])

    def test_development_package_drift_during_startup_is_refused(self):
        original_load = SERVER.ReviewWorkbench._load_package

        def replace_after_load(workbench):
            result = original_load(workbench)
            selection_path = workbench.package / "selection.json"
            selection = json.loads(selection_path.read_text(encoding="utf-8"))
            selection["human_review_status"] = "changed-during-startup"
            selection_path.write_text(json.dumps(selection), encoding="utf-8")
            return result

        with mock.patch.object(
                SERVER.ReviewWorkbench, "_load_package", replace_after_load):
            with self.assertRaisesRegex(
                    SERVER.WorkbenchError, "changed after startup"):
                SERVER.ReviewWorkbench(
                    self.package, self.root / "startup-drift-workspace",
                    self.static_dir, "train")

    def test_pilot_overlap_and_wrong_split_are_rejected(self):
        coco_path = self.package / "train-preannotations.coco.json"
        coco = json.loads(coco_path.read_text(encoding="utf-8"))
        manifest = json.loads(
            (self.package / "development-manifest.json").read_text(encoding="utf-8"))
        pilot = next(item for item in manifest["images"] if item["split"] == "pilot")
        coco["images"][0]["sha256"] = pilot["sha256"]
        coco_path.write_text(json.dumps(coco), encoding="utf-8")
        write_development_evidence_manifest(self.package)
        with self.assertRaisesRegex(SERVER.WorkbenchError, "overlaps pilot"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "overlap-workspace",
                self.static_dir, "train")

        self.package, self.static_dir = make_development_package(
            self.root / "wrong-split")
        coco_path = self.package / "validation-preannotations.coco.json"
        coco = json.loads(coco_path.read_text(encoding="utf-8"))
        coco["images"][0]["split"] = "train"
        coco_path.write_text(json.dumps(coco), encoding="utf-8")
        write_development_evidence_manifest(self.package)
        with self.assertRaisesRegex(SERVER.WorkbenchError, "wrong development split"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "wrong-split-workspace",
                self.static_dir, "validation")

    def test_development_evidence_manifest_status_schema_and_outputs_are_required(self):
        manifest_path = self.package / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["status"] = "FAIL"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(SERVER.WorkbenchError, "PASS status"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "status-workspace",
                self.static_dir, "train")

        self.package, self.static_dir = make_development_package(
            self.root / "bad-outputs")
        manifest_path = self.package / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["outputs"] = []
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(SERVER.WorkbenchError, "output bindings"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "outputs-workspace",
                self.static_dir, "train")

    def test_train_validation_name_or_hash_overlap_is_rejected(self):
        development_path = self.package / "development-manifest.json"
        development = json.loads(development_path.read_text(encoding="utf-8"))
        train = next(item for item in development["images"] if item["split"] == "train")
        validation = next(
            item for item in development["images"] if item["split"] == "validation")
        validation["sha256"] = train["sha256"]
        development_path.write_text(json.dumps(development), encoding="utf-8")
        write_development_evidence_manifest(self.package)
        with self.assertRaisesRegex(
                SERVER.WorkbenchError, "train and validation membership overlaps"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "cross-split-workspace",
                self.static_dir, "train")

    def test_workspace_must_be_disjoint_from_package_and_static_assets(self):
        for workspace in (
                self.package,
                self.package / "train" / "images",
                self.package.parent,
                self.static_dir,
                self.static_dir / "nested",
        ):
            with self.subTest(workspace=workspace):
                with self.assertRaisesRegex(SERVER.WorkbenchError, "must not overlap"):
                    SERVER.ReviewWorkbench(
                        self.package, workspace, self.static_dir, "train")

    def test_launcher_rejects_overlap_before_creating_workspace(self):
        workspace = self.package / "must-not-be-created"
        command = [
            "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(ROOT / "scripts" / "run_windows_p5_development_review.ps1"),
            "-Split", "train", "-Package", str(self.package),
            "-Workspace", str(workspace), "-Port", "8765",
        ]
        result = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not overlap", result.stderr + result.stdout)
        self.assertFalse(workspace.exists())

    def test_launcher_reviewer_mode_requires_existing_pass1(self):
        workspace = self.root / "reviewer-launch-workspace"
        command = [
            "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(ROOT / "scripts" / "run_windows_p5_development_review.ps1"),
            "-Split", "train", "-Mode", "reviewer",
            "-Package", str(self.package), "-Workspace", str(workspace),
            "-Port", "8765",
        ]
        result = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires the immutable pass1", result.stderr + result.stdout)
        self.assertFalse(workspace.exists())

    @unittest.skipUnless(os.name == "nt", "Windows junction behavior")
    def test_launcher_junction_overlap_is_rejected_before_write(self):
        junction = self.root / "package-junction"
        created = subprocess.run(
            [
                "cmd.exe", "/d", "/c", "mklink", "/J",
                str(junction), str(self.package),
            ],
            capture_output=True, text=True, timeout=15,
        )
        if created.returncode != 0:
            self.skipTest(
                f"cannot create test junction: {created.stderr or created.stdout}")
        workspace = junction / "must-not-be-created-through-junction"
        before = {
            path.relative_to(self.package).as_posix():
            hashlib.sha256(path.read_bytes()).hexdigest()
            for path in self.package.rglob("*") if path.is_file()
        }
        command = [
            "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(ROOT / "scripts" / "run_windows_p5_development_review.ps1"),
            "-Split", "train", "-Package", str(self.package),
            "-Workspace", str(workspace), "-Port", "8765",
        ]
        try:
            result = subprocess.run(
                command, cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("must not overlap", result.stderr + result.stdout)
            self.assertFalse(workspace.exists())
            after = {
                path.relative_to(self.package).as_posix():
                hashlib.sha256(path.read_bytes()).hexdigest()
                for path in self.package.rglob("*") if path.is_file()
            }
            self.assertEqual(after, before)
        finally:
            if junction.exists():
                junction.rmdir()

    def test_invalid_development_split_is_rejected(self):
        with self.assertRaisesRegex(SERVER.WorkbenchError, "train or validation"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "bad-workspace",
                self.static_dir, "pilot")


class ReviewerWorkbenchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.package, self.static_dir = make_development_package(self.root)
        annotation_workspace = self.root / "annotation-workspace"
        annotation = SERVER.ReviewWorkbench(
            self.package, annotation_workspace, self.static_dir, "train")
        state = annotation.get_state()
        state["operator_name"] = "标注员 A"
        state["operator_id"] = "annotator-001"
        for image in state["images"]:
            image["review_state"] = "complete"
        annotation.save(state)
        annotation.export_pass1()
        self.pass1 = annotation_workspace / "pass1-train-annotations.coco.json"

        self.workspace = self.root / "reviewer-workspace"
        self.server = SERVER.create_server(
            self.package, self.workspace, port=0,
            static_dir=self.static_dir, split="train", mode="reviewer",
            pass1=self.pass1)
        self.thread = threading.Thread(
            target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        self.temp.cleanup()

    def request(self, method, path, payload=None):
        raw = json.dumps(payload).encode("utf-8") if payload is not None else None
        request = urllib.request.Request(
            self.base + path, data=raw,
            headers={"Content-Type": "application/json"}, method=method)
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                body = response.read()
                return response.status, json.loads(body) if body else None
        except urllib.error.HTTPError as error:
            body = error.read()
            return error.code, json.loads(body) if body else None

    def state(self):
        return self.request("GET", "/api/state")[1]

    def complete_review(self, name="复核员 B", reviewer_id="reviewer-002"):
        state = self.state()
        state["operator_name"] = name
        state["operator_id"] = reviewer_id
        for image in state["images"]:
            image["review_state"] = "complete"
        return state

    def test_reviewer_health_and_state_are_bound_to_pass1(self):
        status, health = self.request("GET", "/api/health")
        self.assertEqual(status, 200)
        self.assertEqual(health["mode"], "reviewer")
        state = self.state()
        self.assertEqual(state["session"]["mode"], "reviewer")
        self.assertEqual(state["session"]["dataset_split"], "train")
        self.assertTrue(all(image["review_state"] == "pending"
                            for image in state["images"]))
        self.assertTrue(all(image["annotated_by"] == "标注员 A"
                            for image in state["images"]))
        self.assertTrue(all(image["annotator_id"] == "annotator-001"
                            for image in state["images"]))
        self.assertTrue(all(len(image["source_content_sha256"]) == 64
                            for image in state["images"]))

    def test_review_candidate_is_complete_auditable_and_still_non_truth(self):
        state = self.complete_review()
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        status, result = self.request("POST", "/api/export-reviewed", {})
        self.assertEqual(status, 200)
        self.assertEqual(result["status"], "independently-reviewed")
        self.assertEqual(result["authorization_status"], "unverified")
        self.assertFalse(result["is_ground_truth"])
        self.assertEqual(result["changed_image_count"], 0)
        candidate = json.loads(Path(result["path"]).read_text(encoding="utf-8"))
        self.assertEqual(
            candidate["info"]["annotation_stage"],
            "independent-review-candidate")
        self.assertFalse(candidate["info"]["ground_truth_complete"])
        self.assertEqual(
            SERVER._sha256(self.pass1),
            candidate["info"]["source_pass1_sha256"])
        self.assertTrue(all(item["is_ground_truth"] is False
                            for item in candidate["images"]))
        self.assertTrue(all(item["authorization_status"] == "unverified"
                            for item in candidate["images"]))
        self.assertTrue(all(item["reviewed_by"] == "复核员 B"
                            for item in candidate["images"]))
        self.assertTrue(all(item["reviewer_id"] == "reviewer-002"
                            for item in candidate["images"]))
        self.assertTrue(all(item["review_outcome"] in {"accepted", "unresolved"}
                            for item in candidate["images"]))
        self.assertTrue(all(item["reviewed_at"]
                            for item in candidate["images"]))
        self.assertTrue(all(item["annotator_id"] == "annotator-001"
                            for item in candidate["annotations"]))
        self.assertTrue(all("score" not in item
                            for item in candidate["annotations"]))

    def test_reviewer_correction_is_recorded_without_mutating_pass1(self):
        before = self.pass1.read_bytes()
        state = self.complete_review()
        target = state["images"][1]
        target["notes"] = "second-person correction"
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        status, result = self.request("POST", "/api/export-reviewed", {})
        self.assertEqual(status, 200)
        self.assertEqual(result["changed_image_count"], 1)
        candidate = json.loads(Path(result["path"]).read_text(encoding="utf-8"))
        corrected = next(
            item for item in candidate["images"] if item["id"] == target["id"])
        self.assertEqual(corrected["review_outcome"], "corrected")
        self.assertTrue(corrected["review_changed"])
        self.assertEqual(self.pass1.read_bytes(), before)

    def test_same_reviewer_identity_is_rejected_after_nfkc_normalization(self):
        state = self.complete_review(
            name=" 标注员　Ａ ", reviewer_id=" ANNOTATOR-001 ")
        status, payload = self.request("POST", "/api/save", state)
        self.assertEqual(status, 409)
        self.assertIn("must differ", payload["error"])
        self.assertFalse(
            (self.workspace / "review-candidate-train.coco.json").exists())

    def test_same_reviewer_identity_is_rejected_with_default_ignorables(self):
        state = self.complete_review(
            name="标注员 A\u200b", reviewer_id="annotator-001\u034f")
        status, payload = self.request("POST", "/api/save", state)
        self.assertEqual(status, 409)
        self.assertIn("must differ", payload["error"])

    def test_reserved_default_ignorables_cannot_bypass_identity_gate(self):
        state = self.complete_review()
        state["operator_name"] = (
            state["images"][0]["annotated_by"] + "\u2065")
        state["operator_id"] = (
            state["images"][0]["annotator_id"] + "\ufff0")
        status, payload = self.request("POST", "/api/save", state)
        self.assertEqual(status, 409)
        self.assertIn("must differ", payload["error"])
        self.assertFalse(
            (self.workspace / "review-candidate-train.coco.json").exists())

    def test_incomplete_review_and_raw_pass1_export_are_refused(self):
        status, payload = self.request("POST", "/api/export-reviewed", {})
        self.assertEqual(status, 409)
        self.assertIn("independently reviewed", payload["error"])
        status, payload = self.request("POST", "/api/export-pass1", {})
        self.assertEqual(status, 409)
        self.assertIn("annotator mode", payload["error"])

    def test_pass1_runtime_drift_is_fail_closed(self):
        pass1 = json.loads(self.pass1.read_text(encoding="utf-8"))
        pass1["images"][0]["notes"] = "tampered"
        self.pass1.write_text(json.dumps(pass1), encoding="utf-8")
        status, payload = self.request("GET", "/api/state")
        self.assertEqual(status, 409)
        self.assertIn("changed after startup", payload["error"])

    def test_workspace_tamper_cannot_change_source_binding_or_outcome(self):
        state_path = self.workspace / "reviewer-state.json"
        state = json.loads(state_path.read_text(encoding="utf-8"))
        state["images"][0]["source_content_sha256"] = "0" * 64
        state_path.write_text(json.dumps(state), encoding="utf-8")
        status, payload = self.request("GET", "/api/state")
        self.assertEqual(status, 409)
        self.assertIn("changed outside this process", payload["error"])

    def test_completed_outcome_and_identity_are_recomputed_from_content(self):
        state = self.complete_review()
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        state_path = self.workspace / "reviewer-state.json"
        saved = json.loads(state_path.read_text(encoding="utf-8"))
        saved["images"][0]["review_outcome"] = "corrected"
        state_path.write_text(json.dumps(saved), encoding="utf-8")
        status, payload = self.request("GET", "/api/state")
        self.assertEqual(status, 409)
        self.assertIn("changed outside this process", payload["error"])

    def test_coherent_completed_disk_rewrite_is_rejected(self):
        state = self.complete_review()
        self.assertEqual(self.request("POST", "/api/save", state)[0], 200)
        state_path = self.workspace / "reviewer-state.json"
        saved = json.loads(state_path.read_text(encoding="utf-8"))
        target = saved["images"][0]
        target["notes"] = "forged but internally coherent"
        target["reviewed_by"] = "forged reviewer"
        target["reviewer_id"] = "forged-reviewer-999"
        target["review_outcome"] = "corrected"
        state_path.write_text(json.dumps(saved), encoding="utf-8")

        status, payload = self.request("GET", "/api/state")
        self.assertEqual(status, 409)
        self.assertIn("changed outside this process", payload["error"])
        status, payload = self.request("POST", "/api/export-reviewed", {})
        self.assertEqual(status, 409)
        self.assertIn("changed outside this process", payload["error"])

    def test_source_annotator_identity_is_rebound_to_pass1_on_restart(self):
        state_path = self.workspace / "reviewer-state.json"
        state = json.loads(state_path.read_text(encoding="utf-8"))
        state["images"][0]["annotated_by"] = "forged-source-name"
        state["images"][0]["annotator_id"] = "forged-source-id"
        state_path.write_text(json.dumps(state), encoding="utf-8")

        with self.assertRaisesRegex(
                SERVER.WorkbenchError,
                "source annotator binding does not match pass1"):
            SERVER.ReviewWorkbench(
                self.package, self.workspace, self.static_dir,
                "train", "reviewer", self.pass1)

    def test_reviewer_workspace_cannot_contain_pass1(self):
        with self.assertRaisesRegex(
                SERVER.WorkbenchError, "must not contain"):
            SERVER.ReviewWorkbench(
                self.package, self.pass1.parent, self.static_dir,
                "train", "reviewer", self.pass1)

    def test_pass1_without_stable_annotator_id_is_rejected(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        pass1 = json.loads(self.pass1.read_text(encoding="utf-8"))
        pass1["info"].pop("annotator_ids")
        for image in pass1["images"]:
            image.pop("annotator_id")
        for annotation in pass1["annotations"]:
            annotation.pop("annotator_id")
        legacy = self.root / "legacy-pass1.json"
        legacy.write_text(json.dumps(pass1), encoding="utf-8")
        with self.assertRaisesRegex(
                SERVER.WorkbenchError, "stable ids"):
            SERVER.ReviewWorkbench(
                self.package, self.root / "legacy-review-workspace",
                self.static_dir, "train", "reviewer", legacy)
        self.server = SERVER.create_server(
            self.package, self.root / "replacement-workspace", port=0,
            static_dir=self.static_dir, split="train", mode="reviewer",
            pass1=self.pass1)
        self.thread = threading.Thread(
            target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"


if __name__ == "__main__":
    unittest.main()
