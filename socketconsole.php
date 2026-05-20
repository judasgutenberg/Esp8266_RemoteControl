<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Device Stream</title>
  <style>
    body {
      font-family: monospace;
      background: #0b0f14;
      color: #d7e1ea;
      margin: 0;
      padding: 0;
    }

    #header {
      padding: 10px;
      background: #111826;
      border-bottom: 1px solid #223;
    }

    #log {
      padding: 10px;
      height: calc(100vh - 50px);
      overflow-y: auto;
      white-space: pre-wrap;
    }

    .msg {
      margin-bottom: 6px;
      padding: 6px;
      border-left: 3px solid #3aa0ff;
    }

    .status {
      color: #7aa2ff;
    }

    .error {
      color: #ff6b6b;
    }
  </style>
</head>
<body>

<div id="header">
  <span>Device Stream:</span>
  <span id="deviceLabel"></span>
  <span id="status" class="status">connecting...</span>
</div>

<div id="log"></div>

<script>
(function () {
  const params = new URLSearchParams(window.location.search);
  const deviceId = params.get("device_id");

  const log = document.getElementById("log");
  const statusEl = document.getElementById("status");
  const deviceLabel = document.getElementById("deviceLabel");

  deviceLabel.textContent = deviceId || "(none)";

  if (!deviceId) {
    statusEl.textContent = "missing device_id";
    statusEl.className = "error";
    return;
  }

  let ws;
  let retryDelay = 1000;

  function addLine(text, cls = "msg") {
    const div = document.createElement("div");
    div.className = cls;
    div.textContent = text;
    log.appendChild(div);
    log.scrollTop = log.scrollHeight;
  }

  function connect() {
    // adjust this to your backend endpoint
    ws = new WebSocket("ws://<?PHP echo $_SERVER['HTTP_HOST']; ?>:8080");

    statusEl.textContent = "connecting...";
    statusEl.className = "status";

    ws.onopen = () => {
      statusEl.textContent = "connected";
      retryDelay = 1000;

      // ?? tell backend what device we want
      ws.send(JSON.stringify({
        type: "subscribe",
        device_id: deviceId
      }));

      addLine("?? Subscribed to device: " + deviceId);
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        // you can format this however your backend sends it
        addLine(JSON.stringify(data, null, 2));
      } catch (e) {
        addLine(event.data);
      }
    };

    ws.onerror = () => {
      addLine("?? socket error", "error");
    };

    ws.onclose = () => {
      statusEl.textContent = "disconnected (reconnecting...)";
      statusEl.className = "error";

      addLine("?? disconnected");

      setTimeout(() => {
        retryDelay = Math.min(retryDelay * 1.5, 10000);
        connect();
      }, retryDelay);
    };
  }

  connect();
})();
</script>

</body>
</html>