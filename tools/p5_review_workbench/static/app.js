"use strict";

(() => {
  const HANDLE_SIZE = 8;
  const MIN_BOX_SIZE = 4;
  const state = {
    session: { mode: "", revision: null, operator_id: "" },
    categories: [],
    images: [],
    activeImageId: null,
    selectedBoxId: null,
    activeTool: "select",
    scaleMode: "fit",
    sourceMode: "original",
    dirty: false,
    saving: false,
    interaction: null,
    imageElement: null,
    imageLoadToken: 0,
    filteredIds: [],
    apiShape: "contract",
  };

  const el = {};
  const ids = [
    "sessionMeta", "progressText", "progressPercent", "progressBar", "operatorId",
    "saveBtn", "exportBtn", "imageCount", "imageSearch", "statusFilter", "imageList",
    "prevBtn", "nextBtn", "queuePosition", "selectTool", "drawTool", "fitBtn",
    "actualBtn", "originalBtn", "previewBtn", "viewportInfo", "deleteBoxBtn",
    "canvasStage", "annotationCanvas", "emptyViewport", "imageLoading", "selectedBoxReadout",
    "predictedDecision", "categorySelect", "categoryState", "boxCount", "boxList", "notes",
    "noteHint", "noteCount", "detailFile", "detailSize", "detailSha", "detailState",
    "completeToggle", "completionHint", "saveStatus", "revisionStatus", "toastRegion",
  ];
  ids.forEach((id) => { el[id] = document.getElementById(id); });
  el.ctx = el.annotationCanvas.getContext("2d");

  function activeImage() {
    return state.images.find((image) => String(image.id) === String(state.activeImageId)) || null;
  }

  function activeBox() {
    const image = activeImage();
    return image?.boxes?.find((box) => String(box.id) === String(state.selectedBoxId)) || null;
  }

  function categoryById(id) {
    return state.categories.find((category) => String(category.id) === String(id));
  }

  function categoryName(id) {
    const category = categoryById(id);
    return category?.display_name || category?.name || category?.model_name || `类别 ${id}`;
  }

  function decisionName(value) {
    return ({ OK: "正常 OK", NG: "缺陷 NG", REVIEW: "待确认 REVIEW" })[String(value || "").toUpperCase()] || "未判定";
  }

  function reviewStateName(value) {
    const key = String(value || "").toLowerCase();
    if (["complete", "completed", "annotation-complete"].includes(key)) return "已完成";
    if (key === "reviewed") return "已复核";
    if (key === "in-progress") return "标注中";
    return "未完成";
  }

  function sourceName(value) {
    return String(value || "").toLowerCase() === "prediction" ? "模型预标注" : "人工标注";
  }

  function modeName(value) {
    const key = String(value || "").toLowerCase();
    if (key === "annotator" || key === "annotation") return "人工首标";
    if (key === "reviewer" || key === "review") return "独立复核";
    return key || "标注";
  }

  function categoryStatusName(value) {
    const key = String(value || "").toLowerCase();
    const names = {
      "source-backed-needs-business-approval": "可试标，待业务批准",
      "unconfirmed-do-not-label": "待业务确认，可框选（仅用于待确认）",
      "forbidden": "禁止标注",
      "approved": "已批准",
    };
    return names[key] || String(value || "").replaceAll("-", " ");
  }
  function categoryDisabled(category) {
    return category?.enabled === false || category?.labelable === false || /forbidden/i.test(String(category?.status || category?.labeling_status || ""));
  }

  function normalizeImage(image) {
    return {
      ...image,
      decision: String(image.decision || "").toUpperCase(),
      predicted_decision: String(image.predicted_decision || "").toUpperCase(),
      review_state: String(image.review_state || "pending").toLowerCase(),
      notes: image.notes || "",
      boxes: Array.isArray(image.boxes) ? image.boxes.map((box, index) => ({
        ...box,
        id: box.id ?? `${image.id}-box-${index + 1}`,
        bbox: Array.isArray(box.bbox) ? box.bbox.map(Number) : [0, 0, 0, 0],
        source: box.source || (state.apiShape === "legacy" ? "prediction" : "human"),
      })) : [],
    };
  }

  async function apiRequest(url, options = {}) {
    const response = await fetch(url, {
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
      ...options,
    });
    const contentType = response.headers.get("content-type") || "";
    const payload = contentType.includes("application/json") ? await response.json() : await response.text();
    if (!response.ok) {
      const message = payload?.error || payload?.message || payload || `${response.status} ${response.statusText}`;
      throw new Error(String(message));
    }
    return payload;
  }

  async function loadState() {
    setStatus("saving", "正在加载标注会话……");
    try {
      const payload = await apiRequest("/api/state");
      applyServerState(payload);
      setStatus("success", "标注会话已加载");
    } catch (error) {
      setStatus("error", `标注会话加载失败：${error.message}`);
      showToast(`无法加载标注会话：${error.message}`, "error", 7000);
      renderAll();
    }
  }

  function applyServerState(payload) {
    state.apiShape = payload.session ? "contract" : "legacy";
    state.session = payload.session
      ? { ...state.session, ...payload.session }
      : { ...state.session, mode: payload.mode, revision: payload.revision, operator_id: payload.operator_id };
    state.categories = Array.isArray(payload.categories) ? payload.categories : [];
    state.images = Array.isArray(payload.images) ? payload.images.map(normalizeImage) : [];
    state.dirty = false;
    el.operatorId.value = state.session.operator_id || "";
    if (!state.images.some((image) => String(image.id) === String(state.activeImageId))) {
      state.activeImageId = state.images[0]?.id ?? null;
    }
    state.selectedBoxId = null;
    populateCategories();
    renderAll();
    loadActiveImage();
  }

  function populateCategories() {
    const previous = el.categorySelect.value;
    el.categorySelect.replaceChildren();
    state.categories.forEach((category) => {
      const option = document.createElement("option");
      option.value = category.id;
      option.textContent = `${category.id}: ${categoryName(category.id)}`;
      option.disabled = categoryDisabled(category);
      el.categorySelect.append(option);
    });
    if ([...el.categorySelect.options].some((option) => option.value === previous && !option.disabled)) {
      el.categorySelect.value = previous;
    } else {
      const firstEnabled = [...el.categorySelect.options].find((option) => !option.disabled);
      if (firstEnabled) el.categorySelect.value = firstEnabled.value;
    }
    updateCategoryState();
  }

  function renderAll() {
    renderHeader();
    renderImageList();
    renderInspector();
    updateToolbar();
    drawCanvas();
  }

  function renderHeader() {
    const total = state.images.length;
    const completed = state.images.filter(isComplete).length;
    const percent = total ? Math.round((completed / total) * 100) : 0;
    el.sessionMeta.textContent = `${modeName(state.session.mode)}模式（仅本机）`;
    el.progressText.textContent = `已完成 ${completed} / ${total} 张`;
    el.progressPercent.textContent = `${percent}%`;
    el.progressBar.style.width = `${percent}%`;
    el.revisionStatus.textContent = `状态版本 ${state.session.revision ?? "-"}`;
    el.saveBtn.disabled = state.saving || !state.images.length;
    el.exportBtn.disabled = state.saving || !state.images.length;
  }

  function imageMatchesFilter(image) {
    const query = el.imageSearch.value.trim().toLowerCase();
    const filter = el.statusFilter.value;
    const textMatch = !query || String(image.file_name || "").toLowerCase().includes(query) || String(image.sha256 || "").toLowerCase().includes(query);
    let statusMatch = true;
    if (filter === "PENDING") statusMatch = !isComplete(image);
    else if (filter === "COMPLETE") statusMatch = isComplete(image);
    else if (filter !== "ALL") statusMatch = image.decision === filter;
    return textMatch && statusMatch;
  }

  function isComplete(image) {
    return ["complete", "completed", "annotation-complete", "reviewed"].includes(String(image.review_state).toLowerCase());
  }

  function renderImageList() {
    const filtered = state.images.filter(imageMatchesFilter);
    state.filteredIds = filtered.map((image) => image.id);
    el.imageList.replaceChildren();
    filtered.forEach((image) => {
      const button = document.createElement("button");
      const decision = decisionName(image.decision);
      button.type = "button";
      button.className = `image-row ${String(image.decision || "").toLowerCase()} ${isComplete(image) ? "complete" : ""} ${String(image.id) === String(state.activeImageId) ? "selected" : ""}`;
      button.dataset.imageId = image.id;
      button.setAttribute("role", "option");
      button.setAttribute("aria-selected", String(String(image.id) === String(state.activeImageId)));
      button.title = image.file_name || `图片 ${image.id}`;
      const stateBar = document.createElement("span");
      stateBar.className = "row-state-bar";
      stateBar.setAttribute("aria-hidden", "true");
      const copy = document.createElement("span");
      copy.className = "row-copy";
      const name = document.createElement("span");
      name.className = "row-name";
      name.textContent = image.file_name || `图片 ${image.id}`;
      const meta = document.createElement("span");
      meta.className = "row-meta";
      meta.textContent = `${image.boxes.length} 个框｜${isComplete(image) ? "已完成" : "未完成"}`;
      copy.append(name, meta);
      const badge = document.createElement("span");
      badge.className = `row-badge ${String(image.decision || "").toLowerCase()}`;
      badge.textContent = decision;
      button.append(stateBar, copy, badge);
      el.imageList.append(button);
    });
    if (!filtered.length) {
      const empty = document.createElement("div");
      empty.className = "empty-list";
      empty.textContent = state.images.length ? "没有符合当前筛选条件的图片。" : "当前标注会话中没有图片。";
      el.imageList.append(empty);
    }
    el.imageCount.textContent = `显示 ${filtered.length} / 共 ${state.images.length} 张`;
    const activeIndex = state.filteredIds.findIndex((id) => String(id) === String(state.activeImageId));
    el.queuePosition.textContent = activeIndex >= 0 ? `第 ${activeIndex + 1} / ${filtered.length} 张` : `第 0 / ${filtered.length} 张`;
    el.prevBtn.disabled = activeIndex <= 0;
    el.nextBtn.disabled = activeIndex < 0 || activeIndex >= filtered.length - 1;
  }

  function renderInspector() {
    const image = activeImage();
    const disabled = !image;
    document.querySelectorAll("[data-decision]").forEach((button) => {
      button.disabled = disabled;
      button.setAttribute("aria-pressed", String(Boolean(image && button.dataset.decision === image.decision)));
    });
    el.predictedDecision.textContent = `模型预测：${image ? decisionName(image.predicted_decision) : "-"}`;
    el.categorySelect.disabled = disabled || !state.categories.length || state.sourceMode !== "original";
    el.notes.disabled = disabled;
    el.completeToggle.disabled = disabled;
    el.completeToggle.checked = Boolean(image && isComplete(image));
    el.notes.value = image?.notes || "";
    el.noteCount.textContent = `${el.notes.value.length} / 2000`;
    el.noteHint.innerHTML = "";
    if (image?.decision === "REVIEW" && !image.notes.trim()) {
      el.noteHint.textContent = "选择“待确认”时必须填写无法判断的原因";
      el.noteHint.className = "warning";
    } else {
      el.noteHint.className = "";
    }
    el.detailFile.textContent = image?.file_name || "-";
    el.detailFile.title = image?.file_name || "";
    el.detailSize.textContent = image ? `${image.width} x ${image.height}` : "-";
    el.detailSha.textContent = image?.sha256 ? `${image.sha256.slice(0, 12)}...` : "-";
    el.detailSha.title = image?.sha256 || "";
    el.detailState.textContent = image ? reviewStateName(image.review_state) : "-";
    el.completionHint.textContent = completionMessage(image);
    renderBoxList();
  }

  function completionMessage(image) {
    if (!image) return "请先从左侧选择一张图片。";
    if (!image.decision) return "请先选择本张图片的结论。";
    if (image.decision === "NG" && !image.boxes.length) return "缺陷（NG）图片至少需要一个缺陷框。";
    if (image.decision === "OK" && image.boxes.length) return "正常（OK）图片不能保留缺陷框。";
    if (image.decision === "REVIEW" && !image.notes.trim()) return "待确认（REVIEW）图片必须填写备注。";
    return isComplete(image) ? "本张图片已计入完成进度。" : "检查无误后可勾选“确认本张标注完成”。";
  }

  function renderBoxList() {
    const image = activeImage();
    const boxes = image?.boxes || [];
    el.boxList.replaceChildren();
    boxes.forEach((box, index) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = `box-row ${box.source === "prediction" ? "prediction" : "human"} ${String(box.id) === String(state.selectedBoxId) ? "selected" : ""}`;
      button.dataset.boxId = box.id;
      button.setAttribute("role", "option");
      button.setAttribute("aria-selected", String(String(box.id) === String(state.selectedBoxId)));
      button.title = `选择第 ${index + 1} 个缺陷框`;
      const swatch = document.createElement("span");
      swatch.className = "box-swatch";
      const copy = document.createElement("span");
      copy.className = "box-copy";
      const className = document.createElement("span");
      className.className = "box-class";
      className.textContent = `${index + 1}. ${categoryName(box.category_id)}`;
      const coords = document.createElement("span");
      coords.className = "box-coords";
      coords.textContent = box.bbox.map((value) => Math.round(value)).join(", ");
      copy.append(className, coords);
      const source = document.createElement("span");
      source.className = "box-source";
      source.textContent = box.source === "prediction" && Number.isFinite(Number(box.score)) ? `模型预标注 ${Math.round(Number(box.score) * 100)}%` : sourceName(box.source);
      button.append(swatch, copy, source);
      el.boxList.append(button);
    });
    if (!boxes.length) {
      const empty = document.createElement("div");
      empty.className = "empty-boxes";
      empty.textContent = "当前没有缺陷框；正常（OK）图片应保持为空。";
      el.boxList.append(empty);
    }
    el.boxCount.textContent = String(boxes.length);
    el.deleteBoxBtn.disabled = state.sourceMode !== "original" || !activeBox();
    const box = activeBox();
    if (box) {
      el.categorySelect.value = String(box.category_id);
      el.selectedBoxReadout.textContent = `${categoryName(box.category_id)} | ${box.bbox.map((value) => Math.round(value)).join(", ")}`;
    } else {
      el.selectedBoxReadout.textContent = "未选择缺陷框";
    }
    updateCategoryState();
  }

  function updateCategoryState() {
    const category = categoryById(el.categorySelect.value);
    const status = category?.labeling_status || category?.status || "";
    el.categoryState.textContent = categoryStatusName(status);
  }

  function updateToolbar() {
    const hasImage = Boolean(activeImage());
    const editable = hasImage && state.sourceMode === "original";
    [el.selectTool, el.drawTool].forEach((button) => {
      const active = button.dataset.tool === state.activeTool;
      button.classList.toggle("active", active);
      button.setAttribute("aria-pressed", String(active));
      button.disabled = !editable;
    });
    el.fitBtn.classList.toggle("active", state.scaleMode === "fit");
    el.actualBtn.classList.toggle("active", state.scaleMode === "actual");
    el.fitBtn.setAttribute("aria-pressed", String(state.scaleMode === "fit"));
    el.actualBtn.setAttribute("aria-pressed", String(state.scaleMode === "actual"));
    el.fitBtn.disabled = !hasImage;
    el.actualBtn.disabled = !hasImage;
    el.originalBtn.classList.toggle("active", state.sourceMode === "original");
    el.previewBtn.classList.toggle("active", state.sourceMode === "preview");
    el.originalBtn.setAttribute("aria-pressed", String(state.sourceMode === "original"));
    el.previewBtn.setAttribute("aria-pressed", String(state.sourceMode === "preview"));
    el.originalBtn.disabled = !hasImage;
    el.previewBtn.disabled = !hasImage || !activeImage()?.preview_url;
    el.canvasStage.classList.toggle("drawing", state.activeTool === "draw");
  }

  function selectImage(id) {
    if (String(state.activeImageId) === String(id)) return;
    state.activeImageId = id;
    state.selectedBoxId = null;
    state.interaction = null;
    renderAll();
    loadActiveImage();
    requestAnimationFrame(() => {
      el.imageList.querySelector(".image-row.selected")?.scrollIntoView({ block: "nearest" });
      el.canvasStage.focus({ preventScroll: true });
    });
  }

  function navigate(direction) {
    const ids = state.filteredIds.length ? state.filteredIds : state.images.map((image) => image.id);
    const index = ids.findIndex((id) => String(id) === String(state.activeImageId));
    const next = Math.max(0, Math.min(ids.length - 1, index + direction));
    if (ids[next] != null && next !== index) selectImage(ids[next]);
  }

  function mutateImage(mutator, options = {}) {
    const image = activeImage();
    if (!image) return;
    mutator(image);
    if (options.markDirty !== false) markDirty();
    renderAll();
  }

  function markDirty() {
    state.dirty = true;
    setStatus("unsaved", "有尚未保存的更改");
  }

  function setStatus(type, message) {
    el.saveStatus.className = `save-status ${type || ""}`;
    el.saveStatus.lastElementChild.textContent = message;
  }

  function showToast(message, type = "", duration = 3500) {
    const toast = document.createElement("div");
    toast.className = `toast ${type}`;
    toast.textContent = message;
    el.toastRegion.append(toast);
    window.setTimeout(() => toast.remove(), duration);
  }

  function validationError(image) {
    if (!image.decision) return "请先选择正常（OK）、缺陷（NG）或待确认（REVIEW）。";
    if (image.decision === "NG" && !image.boxes.length) return "缺陷（NG）图片至少需要一个缺陷框。";
    if (image.decision === "OK" && image.boxes.length) return "标记为正常（OK）前，请删除所有缺陷框。";
    if (image.decision === "REVIEW" && !image.notes.trim()) return "请填写本张图片需要待确认的原因。";
    return "";
  }

  async function save() {
    if (state.saving) return;
    const operatorId = el.operatorId.value.trim();
    if (!operatorId) {
      el.operatorId.focus();
      showToast("保存前必须填写真实的标注人 ID。", "error");
      return;
    }
    state.saving = true;
    renderHeader();
    setStatus("saving", "正在保存更改……");
    try {
      const payload = await apiRequest("/api/save", {
        method: "POST",
        body: JSON.stringify(savePayload(operatorId)),
      });
      const currentId = state.activeImageId;
      applyServerState(payload);
      if (state.images.some((image) => String(image.id) === String(currentId))) state.activeImageId = currentId;
      state.dirty = false;
      setStatus("success", "所有更改已保存");
      showToast("标注已保存。", "success");
      renderAll();
      loadActiveImage();
    } catch (error) {
      setStatus("error", `保存失败：${error.message}`);
      showToast(`保存失败：${error.message}`, "error", 7000);
    } finally {
      state.saving = false;
      renderHeader();
    }
  }

  function editableImage(image) {
    return {
      id: image.id,
      decision: image.decision,
      review_state: image.review_state,
      notes: image.notes,
      boxes: image.boxes.map((box) => ({
        id: box.id,
        category_id: box.category_id,
        bbox: box.bbox.map((value) => Number(value)),
        source: box.source,
        ...(box.score == null ? {} : { score: box.score }),
      })),
    };
  }

  function savePayload(operatorId) {
    if (state.apiShape === "legacy") {
      return {
        revision: state.session.revision,
        mode: state.session.mode,
        operator_id: operatorId,
        categories: state.categories,
        images: state.images.map((image) => ({ ...image, boxes: image.boxes.map((box) => ({ ...box, bbox: box.bbox.map(Number) })) })),
      };
    }
    return {
      revision: state.session.revision,
      operator_id: operatorId,
      images: state.images.map(editableImage),
    };
  }

  async function exportPass1() {
    if (state.dirty) {
      const shouldSave = window.confirm("当前有尚未保存的更改，是否先保存再导出？");
      if (!shouldSave) return;
      await save();
      if (state.dirty) return;
    }
    const incomplete = state.images.filter((image) => !isComplete(image)).length;
    if (incomplete) {
      showToast(`还有 ${incomplete} 张图片未完成。请完成并保存全部图片后再导出。`, "error", 6000);
      return;
    }
    state.saving = true;
    renderHeader();
    setStatus("saving", "正在导出第一轮人工标注……");
    try {
      const payload = await apiRequest("/api/export-pass1", { method: "POST", body: JSON.stringify({}) });
      const message = payload?.path ? `首标已导出：${payload.path}` : (payload?.message || "首标导出完成。");
      setStatus("success", String(message));
      showToast(String(message), "success", 5000);
    } catch (error) {
      setStatus("error", `导出失败：${error.message}`);
      showToast(`导出失败：${error.message}`, "error", 7000);
    } finally {
      state.saving = false;
      renderHeader();
    }
  }

  function canvasGeometry() {
    const image = activeImage();
    if (!image || !state.imageElement) return null;
    const stageWidth = el.canvasStage.clientWidth;
    const stageHeight = el.canvasStage.clientHeight;
    const pad = 22;
    const scale = state.scaleMode === "actual" ? 1 : Math.min((stageWidth - pad * 2) / image.width, (stageHeight - pad * 2) / image.height);
    const safeScale = Math.max(0.01, scale);
    const width = Math.round(image.width * safeScale);
    const height = Math.round(image.height * safeScale);
    const stageContentWidth = Math.max(stageWidth, width + pad * 2);
    const stageContentHeight = Math.max(stageHeight, height + pad * 2);
    const left = Math.round((stageContentWidth - width) / 2);
    const top = Math.round((stageContentHeight - height) / 2);
    return { scale: safeScale, width, height, left, top, contentWidth: stageContentWidth, contentHeight: stageContentHeight };
  }

  function applyCanvasGeometry(geometry) {
    const dpr = window.devicePixelRatio || 1;
    el.annotationCanvas.style.left = `${geometry.left}px`;
    el.annotationCanvas.style.top = `${geometry.top}px`;
    el.annotationCanvas.style.width = `${geometry.width}px`;
    el.annotationCanvas.style.height = `${geometry.height}px`;
    el.annotationCanvas.width = Math.max(1, Math.round(geometry.width * dpr));
    el.annotationCanvas.height = Math.max(1, Math.round(geometry.height * dpr));
    el.canvasStage.style.setProperty("--content-width", `${geometry.contentWidth}px`);
    el.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  function drawCanvas() {
    const geometry = canvasGeometry();
    if (!geometry) {
      el.ctx.clearRect(0, 0, el.annotationCanvas.width, el.annotationCanvas.height);
      el.emptyViewport.classList.toggle("hidden", Boolean(activeImage()));
      el.viewportInfo.textContent = activeImage() ? `${activeImage().width} × ${activeImage().height}` : "尚未选择图片";
      return;
    }
    applyCanvasGeometry(geometry);
    el.ctx.clearRect(0, 0, geometry.width, geometry.height);
    el.ctx.drawImage(state.imageElement, 0, 0, geometry.width, geometry.height);
    if (state.sourceMode === "original") {
      activeImage().boxes.forEach((box) => drawBox(box, geometry, String(box.id) === String(state.selectedBoxId)));
      if (state.interaction?.kind === "draw") drawDraftBox(state.interaction, geometry);
    }
    el.emptyViewport.classList.add("hidden");
    el.viewportInfo.textContent = `${activeImage().width} x ${activeImage().height} | ${Math.round(geometry.scale * 100)}%`;
  }

  function drawBox(box, geometry, selected) {
    const [x, y, width, height] = box.bbox;
    const sx = x * geometry.scale;
    const sy = y * geometry.scale;
    const sw = width * geometry.scale;
    const sh = height * geometry.scale;
    const color = box.source === "prediction" ? "#dc5559" : "#43bd72";
    el.ctx.save();
    el.ctx.strokeStyle = selected ? "#65a9d5" : color;
    el.ctx.lineWidth = selected ? 2 : 1.5;
    el.ctx.setLineDash(box.source === "prediction" ? [5, 3] : []);
    el.ctx.strokeRect(Math.round(sx) + .5, Math.round(sy) + .5, Math.round(sw), Math.round(sh));
    const label = categoryName(box.category_id);
    el.ctx.font = "600 11px Segoe UI";
    const labelWidth = Math.min(sw, el.ctx.measureText(label).width + 10);
    if (labelWidth > 18) {
      el.ctx.fillStyle = selected ? "#2e6f9f" : color;
      el.ctx.fillRect(sx, Math.max(0, sy - 18), labelWidth, 18);
      el.ctx.fillStyle = "#ffffff";
      el.ctx.fillText(label, sx + 5, Math.max(12, sy - 5), labelWidth - 8);
    }
    if (selected) drawHandles(sx, sy, sw, sh);
    el.ctx.restore();
  }

  function drawHandles(x, y, width, height) {
    el.ctx.fillStyle = "#eef7fb";
    el.ctx.strokeStyle = "#2e6f9f";
    handlePoints(x, y, width, height).forEach((point) => {
      el.ctx.fillRect(point.x - HANDLE_SIZE / 2, point.y - HANDLE_SIZE / 2, HANDLE_SIZE, HANDLE_SIZE);
      el.ctx.strokeRect(point.x - HANDLE_SIZE / 2, point.y - HANDLE_SIZE / 2, HANDLE_SIZE, HANDLE_SIZE);
    });
  }

  function drawDraftBox(interaction, geometry) {
    const x = Math.min(interaction.start.x, interaction.current.x) * geometry.scale;
    const y = Math.min(interaction.start.y, interaction.current.y) * geometry.scale;
    const width = Math.abs(interaction.current.x - interaction.start.x) * geometry.scale;
    const height = Math.abs(interaction.current.y - interaction.start.y) * geometry.scale;
    el.ctx.save();
    el.ctx.strokeStyle = "#65a9d5";
    el.ctx.fillStyle = "rgb(46 111 159 / 18%)";
    el.ctx.lineWidth = 2;
    el.ctx.fillRect(x, y, width, height);
    el.ctx.strokeRect(x, y, width, height);
    el.ctx.restore();
  }

  function handlePoints(x, y, width, height) {
    return [
      { name: "nw", x, y }, { name: "n", x: x + width / 2, y }, { name: "ne", x: x + width, y },
      { name: "e", x: x + width, y: y + height / 2 }, { name: "se", x: x + width, y: y + height },
      { name: "s", x: x + width / 2, y: y + height }, { name: "sw", x, y: y + height }, { name: "w", x, y: y + height / 2 },
    ];
  }

  function canvasPoint(event) {
    const geometry = canvasGeometry();
    if (!geometry) return null;
    const rect = el.annotationCanvas.getBoundingClientRect();
    return {
      x: clamp((event.clientX - rect.left) / geometry.scale, 0, activeImage().width),
      y: clamp((event.clientY - rect.top) / geometry.scale, 0, activeImage().height),
      canvasX: event.clientX - rect.left,
      canvasY: event.clientY - rect.top,
      geometry,
    };
  }

  function hitTest(point) {
    const image = activeImage();
    if (!image) return null;
    const selected = activeBox();
    if (selected) {
      const [x, y, width, height] = selected.bbox.map((value) => value * point.geometry.scale);
      const handle = handlePoints(x, y, width, height).find((candidate) => Math.abs(point.canvasX - candidate.x) <= HANDLE_SIZE && Math.abs(point.canvasY - candidate.y) <= HANDLE_SIZE);
      if (handle) return { box: selected, handle: handle.name };
    }
    for (let index = image.boxes.length - 1; index >= 0; index -= 1) {
      const box = image.boxes[index];
      const [x, y, width, height] = box.bbox;
      if (point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height) return { box, handle: null };
    }
    return null;
  }

  function onPointerDown(event) {
    if (event.button !== 0 || !state.imageElement || state.sourceMode !== "original") return;
    const point = canvasPoint(event);
    if (!point) return;
    el.annotationCanvas.setPointerCapture(event.pointerId);
    if (state.activeTool === "draw") {
      state.interaction = { kind: "draw", start: point, current: point };
      drawCanvas();
      return;
    }
    const hit = hitTest(point);
    if (!hit) {
      state.selectedBoxId = null;
      renderInspector();
      drawCanvas();
      return;
    }
    state.selectedBoxId = hit.box.id;
    state.interaction = {
      kind: hit.handle ? "resize" : "move",
      handle: hit.handle,
      start: point,
      original: [...hit.box.bbox],
      boxId: hit.box.id,
      changed: false,
    };
    el.canvasStage.classList.add("grabbing");
    renderInspector();
    drawCanvas();
  }

  function onPointerMove(event) {
    if (!state.interaction) return;
    const point = canvasPoint(event);
    if (!point) return;
    if (state.interaction.kind === "draw") {
      state.interaction.current = point;
      drawCanvas();
      return;
    }
    const box = activeImage()?.boxes.find((candidate) => String(candidate.id) === String(state.interaction.boxId));
    if (!box) return;
    if (state.interaction.kind === "move") moveBox(box, point);
    else resizeBox(box, point);
    state.interaction.changed = true;
    drawCanvas();
    renderBoxList();
  }

  function onPointerUp(event) {
    if (!state.interaction) return;
    const interaction = state.interaction;
    const point = canvasPoint(event);
    state.interaction = null;
    el.canvasStage.classList.remove("grabbing");
    if (interaction.kind === "draw" && point) finishDraw(interaction.start, point);
    else if (interaction.changed) {
      activeImage().review_state = "pending";
      markDirty();
    }
    renderAll();
  }

  function moveBox(box, point) {
    const [x, y, width, height] = state.interaction.original;
    const dx = point.x - state.interaction.start.x;
    const dy = point.y - state.interaction.start.y;
    box.bbox = [clamp(x + dx, 0, activeImage().width - width), clamp(y + dy, 0, activeImage().height - height), width, height];
  }

  function resizeBox(box, point) {
    const [x, y, width, height] = state.interaction.original;
    let left = x;
    let top = y;
    let right = x + width;
    let bottom = y + height;
    const handle = state.interaction.handle;
    if (handle.includes("w")) left = clamp(point.x, 0, right - MIN_BOX_SIZE);
    if (handle.includes("e")) right = clamp(point.x, left + MIN_BOX_SIZE, activeImage().width);
    if (handle.includes("n")) top = clamp(point.y, 0, bottom - MIN_BOX_SIZE);
    if (handle.includes("s")) bottom = clamp(point.y, top + MIN_BOX_SIZE, activeImage().height);
    box.bbox = [left, top, right - left, bottom - top];
  }

  function finishDraw(start, end) {
    const x = Math.min(start.x, end.x);
    const y = Math.min(start.y, end.y);
    const width = Math.abs(end.x - start.x);
    const height = Math.abs(end.y - start.y);
    if (width < MIN_BOX_SIZE || height < MIN_BOX_SIZE) {
      showToast("缺陷框太小，请拖动鼠标框住可见缺陷。", "error");
      return;
    }
    const category = categoryById(el.categorySelect.value);
    if (!category || categoryDisabled(category)) {
      showToast("绘制前请先选择允许标注的缺陷类别。", "error");
      return;
    }
    const box = { id: makeBoxId(), category_id: category.id, bbox: [x, y, width, height], source: "human" };
    activeImage().boxes.push(box);
    activeImage().decision = activeImage().boxes.some((item) => [7, 8].includes(Number(item.category_id))) ? "REVIEW" : "NG";
    activeImage().review_state = "pending";
    state.selectedBoxId = box.id;
    markDirty();
  }

  function makeBoxId() {
    if (state.apiShape === "legacy") {
      return state.images.reduce((highest, image) => image.boxes.reduce((boxHighest, box) => Math.max(boxHighest, Number(box.id) || 0), highest), 0) + 1;
    }
    if (globalThis.crypto?.randomUUID) return globalThis.crypto.randomUUID();
    return `box-${Date.now()}-${Math.random().toString(16).slice(2)}`;
  }

  function deleteSelectedBox() {
    if (state.sourceMode !== "original") return;
    const image = activeImage();
    const box = activeBox();
    if (!image || !box) return;
    if (!window.confirm(`确定删除选中的“${categoryName(box.category_id)}”缺陷框吗？`)) return;
    image.boxes = image.boxes.filter((candidate) => String(candidate.id) !== String(box.id));
    state.selectedBoxId = null;
    image.review_state = "pending";
    markDirty();
    renderAll();
  }

  async function loadActiveImage() {
    const image = activeImage();
    const token = ++state.imageLoadToken;
    state.imageElement = null;
    el.imageLoading.classList.toggle("hidden", !image);
    drawCanvas();
    if (!image) return;
    const url = state.sourceMode === "preview" && image.preview_url ? image.preview_url : image.image_url;
    if (!url) {
      el.imageLoading.classList.add("hidden");
      showToast("本张图片没有可用的显示地址。", "error");
      return;
    }
    const bitmap = new Image();
    bitmap.decoding = "async";
    bitmap.onload = () => {
      if (token !== state.imageLoadToken) return;
      state.imageElement = bitmap;
      el.imageLoading.classList.add("hidden");
      drawCanvas();
    };
    bitmap.onerror = () => {
      if (token !== state.imageLoadToken) return;
      el.imageLoading.classList.add("hidden");
      setStatus("error", `无法加载图片：${image.file_name}`);
      showToast(`无法加载图片：${image.file_name}`, "error");
    };
    bitmap.src = url;
  }

  function clamp(value, min, max) { return Math.min(max, Math.max(min, value)); }

  function bindEvents() {
    el.imageList.addEventListener("click", (event) => {
      const row = event.target.closest("[data-image-id]");
      if (row) selectImage(row.dataset.imageId);
    });
    el.boxList.addEventListener("click", (event) => {
      const row = event.target.closest("[data-box-id]");
      if (!row) return;
      state.selectedBoxId = row.dataset.boxId;
      state.activeTool = "select";
      renderAll();
      el.canvasStage.focus({ preventScroll: true });
    });
    el.imageSearch.addEventListener("input", renderImageList);
    el.statusFilter.addEventListener("change", renderImageList);
    el.prevBtn.addEventListener("click", () => navigate(-1));
    el.nextBtn.addEventListener("click", () => navigate(1));
    el.saveBtn.addEventListener("click", save);
    el.exportBtn.addEventListener("click", exportPass1);
    el.operatorId.addEventListener("input", () => {
      state.session.operator_id = el.operatorId.value;
      markDirty();
    });
    document.querySelectorAll("[data-tool]").forEach((button) => button.addEventListener("click", () => {
      state.activeTool = button.dataset.tool;
      state.interaction = null;
      updateToolbar();
      drawCanvas();
      el.canvasStage.focus({ preventScroll: true });
    }));
    document.querySelectorAll("[data-decision]").forEach((button) => button.addEventListener("click", () => {
      const image = activeImage();
      if (!image) return;
      const decision = button.dataset.decision;
      const mustClearBoxes = decision === "OK";
      if (mustClearBoxes && image.boxes.length && !window.confirm(`确定将本张设为“${decisionName(decision)}”并删除全部 ${image.boxes.length} 个不兼容缺陷框吗？`)) return;
      if (mustClearBoxes) {
        image.boxes = [];
        state.selectedBoxId = null;
      }
      image.decision = decision;
      image.review_state = "pending";
      markDirty();
      renderAll();
    }));
    el.categorySelect.addEventListener("change", () => {
      if (state.sourceMode !== "original") return;
      const box = activeBox();
      if (box) {
        box.category_id = categoryById(el.categorySelect.value)?.id ?? el.categorySelect.value;
        activeImage().decision = activeImage().boxes.some((item) => [7, 8].includes(Number(item.category_id))) ? "REVIEW" : "NG";
        box.source = "human";
        delete box.score;
        activeImage().review_state = "pending";
        markDirty();
        renderAll();
      } else updateCategoryState();
    });
    el.notes.addEventListener("input", () => {
      const image = activeImage();
      if (!image) return;
      image.notes = el.notes.value;
      image.review_state = "pending";
      markDirty();
      renderHeader();
      el.noteCount.textContent = `${el.notes.value.length} / 2000`;
      el.noteHint.textContent = image.decision === "REVIEW" && !image.notes.trim() ? "选择“待确认”时必须填写无法判断的原因" : "";
      el.noteHint.className = image.decision === "REVIEW" && !image.notes.trim() ? "warning" : "";
      el.completionHint.textContent = completionMessage(image);
    });
    el.completeToggle.addEventListener("change", () => {
      const image = activeImage();
      if (!image) return;
      if (el.completeToggle.checked) {
        const error = validationError(image);
        if (error) {
          el.completeToggle.checked = false;
          showToast(error, "error");
          return;
        }
        image.review_state = state.apiShape === "legacy" ? "annotation-complete" : "complete";
      } else if (window.confirm("确定重新打开本张图片进行编辑吗？")) {
        image.review_state = "pending";
      } else {
        el.completeToggle.checked = true;
        return;
      }
      markDirty();
      renderAll();
    });
    el.fitBtn.addEventListener("click", () => setScaleMode("fit"));
    el.actualBtn.addEventListener("click", () => setScaleMode("actual"));
    el.originalBtn.addEventListener("click", () => setSourceMode("original"));
    el.previewBtn.addEventListener("click", () => setSourceMode("preview"));
    el.deleteBoxBtn.addEventListener("click", deleteSelectedBox);
    el.annotationCanvas.addEventListener("pointerdown", onPointerDown);
    el.annotationCanvas.addEventListener("pointermove", onPointerMove);
    el.annotationCanvas.addEventListener("pointerup", onPointerUp);
    el.annotationCanvas.addEventListener("pointercancel", () => { state.interaction = null; drawCanvas(); });
    window.addEventListener("resize", drawCanvas);
    window.addEventListener("beforeunload", (event) => {
      if (!state.dirty) return;
      event.preventDefault();
      event.returnValue = "";
    });
    document.addEventListener("keydown", onKeyDown);
  }

  function setScaleMode(mode) {
    state.scaleMode = mode;
    updateToolbar();
    drawCanvas();
  }

  function setSourceMode(mode) {
    if (mode === "preview" && !activeImage()?.preview_url) return;
    state.sourceMode = mode;
    updateToolbar();
    renderInspector();
    loadActiveImage();
  }

  function onKeyDown(event) {
    const editingText = ["INPUT", "TEXTAREA", "SELECT"].includes(document.activeElement?.tagName);
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
      event.preventDefault();
      save();
      return;
    }
    if (editingText) return;
    if (event.key === "ArrowLeft" || event.key === "ArrowUp") {
      event.preventDefault();
      navigate(-1);
    } else if (event.key === "ArrowRight" || event.key === "ArrowDown") {
      event.preventDefault();
      navigate(1);
    } else if ((event.key === "Delete" || event.key === "Backspace")
               && state.sourceMode === "original") {
      event.preventDefault();
      deleteSelectedBox();
    } else if (event.key.toLowerCase() === "v") {
      state.activeTool = "select";
      updateToolbar();
    } else if (event.key.toLowerCase() === "b") {
      state.activeTool = "draw";
      updateToolbar();
    }
  }

  bindEvents();
  loadState();
})();
