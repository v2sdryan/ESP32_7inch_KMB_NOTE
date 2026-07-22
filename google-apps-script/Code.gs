const MAX_TASKS = 12;

function jsonResponse(payload) {
  return ContentService.createTextOutput(JSON.stringify(payload))
      .setMimeType(ContentService.MimeType.JSON);
}

function constantTimeEqual(left, right) {
  const a = Utilities.computeDigest(Utilities.DigestAlgorithm.SHA_256, left || "");
  const b = Utilities.computeDigest(Utilities.DigestAlgorithm.SHA_256, right || "");
  if (a.length !== b.length) return false;
  let difference = 0;
  for (let i = 0; i < a.length; i += 1) difference |= a[i] ^ b[i];
  return difference === 0;
}

function doPost(event) {
  try {
    const request = JSON.parse((event && event.postData && event.postData.contents) || "{}");
    const expectedToken = PropertiesService.getScriptProperties().getProperty("DEVICE_TOKEN") || "";
    if (!expectedToken || !constantTimeEqual(String(request.device_token || ""), expectedToken)) {
      return jsonResponse({ok: false, error: "unauthorized"});
    }

    const date = String(request.date || "");
    const tasklistId = String(request.tasklist_id || "@default");
    if (!/^\d{4}-\d{2}-\d{2}$/.test(date) || tasklistId.length > 128) {
      return jsonResponse({ok: false, error: "invalid_request"});
    }

    const dueMin = new Date(date + "T00:00:00+08:00").toISOString();
    const nextDay = new Date(date + "T00:00:00+08:00");
    nextDay.setUTCDate(nextDay.getUTCDate() + 1);
    const result = Tasks.Tasks.list(tasklistId, {
      dueMin: dueMin,
      dueMax: nextDay.toISOString(),
      showCompleted: false,
      showHidden: false,
      maxResults: MAX_TASKS
    });

    const tasks = (result.items || []).slice(0, MAX_TASKS).map(function(task) {
      return {
        title: String(task.title || "").slice(0, 72),
        due: String(task.due || ""),
        completed: task.status === "completed"
      };
    });
    return jsonResponse({ok: true, tasks: tasks});
  } catch (error) {
    console.error("Google Tasks bridge error", error);
    return jsonResponse({ok: false, error: "bridge_error"});
  }
}
