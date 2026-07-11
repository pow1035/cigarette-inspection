import copy
import csv
import hashlib
import http.client
import importlib.util
import json
import struct
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import zlib
from pathlib import Path


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

    def test_unconfirmed_class_cannot_be_exported_as_ng(self):
        state = self.complete(self.state())
        state["images"][0]["boxes"][0]["category_id"] = 7
        status, payload = self.request("POST", "/api/save", state)
        self.assertEqual(status, 400)
        self.assertIn("unconfirmed", payload["error"])
        self.assertFalse((self.workspace / "pass1-annotations.coco.json").exists())

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


if __name__ == "__main__":
    unittest.main()
