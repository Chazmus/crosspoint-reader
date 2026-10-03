/**
 * CrossPoint Web Flasher — Application Logic & Web Serial Integration
 */

// ============================================================================
// 1. Tab Navigation & Initialization
// ============================================================================
document.addEventListener("DOMContentLoaded", () => {
  initTabs();
  initDeviceSelector();
  initBrowserCheck();
  loadMetadata();
  initCustomFlasher();
  initSerialMonitor();
});

function initTabs() {
  const tabBtns = document.querySelectorAll(".tab-btn");
  const tabContents = document.querySelectorAll(".tab-content");

  tabBtns.forEach((btn) => {
    btn.addEventListener("click", () => {
      const targetTab = btn.getAttribute("data-tab");

      tabBtns.forEach((b) => {
        b.classList.remove("active");
        b.setAttribute("aria-selected", "false");
      });
      tabContents.forEach((c) => c.classList.remove("active"));

      btn.classList.add("active");
      btn.setAttribute("aria-selected", "true");
      const activeContent = document.getElementById(targetTab);
      if (activeContent) {
        activeContent.classList.add("active");
      }
    });
  });
}

// ============================================================================
// 2. Browser Compatibility Check
// ============================================================================
function initBrowserCheck() {
  const hasSerial = "serial" in navigator;
  const badge = document.getElementById("badgeSerial");
  const alertBox = document.getElementById("unsupportedAlert");

  if (hasSerial) {
    badge.textContent = "Web Serial: Supported";
    badge.classList.add("badge-accent");
    alertBox.style.display = "none";
  } else {
    badge.textContent = "Web Serial: Unavailable";
    badge.classList.remove("badge-accent");
    alertBox.style.display = "flex";
  }
}

// ============================================================================
// 3. Device Selector & Manifest Switcher
// ============================================================================
function initDeviceSelector() {
  const cards = document.querySelectorAll(".device-card");
  const targetName = document.getElementById("targetName");
  const targetChip = document.getElementById("targetChip");
  const targetFile = document.getElementById("targetFile");
  const btnDownloadBin = document.getElementById("btnDownloadBin");
  const espWebInstallBtn = document.getElementById("espWebInstallBtn");

  cards.forEach((card) => {
    card.addEventListener("click", () => {
      cards.forEach((c) => c.classList.remove("selected"));
      card.classList.add("selected");

      const name = card.getAttribute("data-name");
      const chip = card.getAttribute("data-chip");
      const file = card.getAttribute("data-file");
      const manifest = card.getAttribute("data-manifest");

      if (targetName) targetName.textContent = name;
      if (targetChip) targetChip.textContent = chip;
      if (targetFile) targetFile.textContent = file;

      if (btnDownloadBin) {
        btnDownloadBin.href = `./firmware/${file}`;
        btnDownloadBin.setAttribute("download", file);
      }

      if (espWebInstallBtn && manifest) {
        espWebInstallBtn.setAttribute("manifest", manifest);
      }
    });
  });
}

// ============================================================================
// 4. Metadata & Version Loader
// ============================================================================
async function loadMetadata() {
  try {
    const res = await fetch("./version.json?t=" + Date.now());
    if (!res.ok) return;
    const data = await res.json();

    if (data.branch) {
      document.getElementById("badgeBranch").textContent = `Branch: ${data.branch}`;
    }
    if (data.commit) {
      document.getElementById("badgeCommit").textContent = `Commit: ${data.commit}`;
    }
  } catch (err) {
    console.debug("version.json not loaded yet:", err);
  }
}

// ============================================================================
// 5. Custom .bin Flasher (esptool-js integration)
// ============================================================================
let customFileBuffer = null;
let customFileName = "";

function initCustomFlasher() {
  const dropzone = document.getElementById("binDropzone");
  const fileInput = document.getElementById("binFileInput");
  const fileInfo = document.getElementById("selectedFileInfo");
  const btnFlash = document.getElementById("btnFlashCustom");
  const offsetInput = document.getElementById("flashOffset");
  const presetBtns = document.querySelectorAll(".preset-btn");
  const btnClearLog = document.getElementById("btnClearCustomLog");
  const terminal = document.getElementById("customTerminal");

  presetBtns.forEach((btn) => {
    btn.addEventListener("click", () => {
      offsetInput.value = btn.getAttribute("data-offset");
    });
  });

  if (btnClearLog) {
    btnClearLog.addEventListener("click", () => {
      terminal.textContent = "";
    });
  }

  function handleFile(file) {
    if (!file) return;
    customFileName = file.name;
    const sizeKb = (file.size / 1024).toFixed(1);
    fileInfo.textContent = `${file.name} (${sizeKb} KB)`;
    btnFlash.disabled = false;

    const reader = new FileReader();
    reader.onload = (e) => {
      customFileBuffer = e.target.result;
      logCustom(`Loaded file: ${file.name} (${file.size} bytes)`);
    };
    reader.readAsArrayBuffer(file);
  }

  if (fileInput) {
    fileInput.addEventListener("change", (e) => {
      if (e.target.files && e.target.files[0]) {
        handleFile(e.target.files[0]);
      }
    });
  }

  if (dropzone) {
    ["dragenter", "dragover"].forEach((eventName) => {
      dropzone.addEventListener(eventName, (e) => {
        e.preventDefault();
        e.stopPropagation();
        dropzone.classList.add("dragover");
      });
    });

    ["dragleave", "drop"].forEach((eventName) => {
      dropzone.addEventListener(eventName, (e) => {
        e.preventDefault();
        e.stopPropagation();
        dropzone.classList.remove("dragover");
      });
    });

    dropzone.addEventListener("drop", (e) => {
      if (e.dataTransfer.files && e.dataTransfer.files[0]) {
        handleFile(e.dataTransfer.files[0]);
      }
    });
  }

  if (btnFlash) {
    btnFlash.addEventListener("click", async () => {
      if (!customFileBuffer) {
        alert("Please select a .bin file first.");
        return;
      }
      if (!("serial" in navigator)) {
        alert("Web Serial is not supported in this browser. Please use Google Chrome, Edge, or Brave.");
        return;
      }

      await runCustomFlash();
    });
  }
}

function logCustom(msg) {
  const terminal = document.getElementById("customTerminal");
  if (!terminal) return;
  const time = new Date().toLocaleTimeString();
  terminal.textContent += `[${time}] ${msg}\n`;
  terminal.scrollTop = terminal.scrollHeight;
}

async function runCustomFlash() {
  const btnFlash = document.getElementById("btnFlashCustom");
  const progressBox = document.getElementById("flashProgressBox");
  const progressBar = document.getElementById("flashProgressBar");
  const progressText = document.getElementById("flashProgressText");
  const progressPct = document.getElementById("flashProgressPct");
  const offsetStr = document.getElementById("flashOffset").value.trim();
  const baudRate = parseInt(document.getElementById("flashBaud").value, 10);
  const eraseAll = document.getElementById("eraseAllCheckbox").checked;

  let offset = 0x10000;
  if (offsetStr.startsWith("0x") || offsetStr.startsWith("0X")) {
    offset = parseInt(offsetStr, 16);
  } else {
    offset = parseInt(offsetStr, 10);
  }
  if (isNaN(offset)) {
    alert("Invalid flash offset address.");
    return;
  }

  btnFlash.disabled = true;
  progressBox.style.display = "block";
  progressBar.style.width = "0%";
  progressText.textContent = "Connecting to device...";
  progressPct.textContent = "0%";

  let transport = null;
  let esploader = null;

  try {
    logCustom("Requesting Serial Port...");
    const port = await navigator.serial.requestPort();

    logCustom("Loading esptool-js engine...");
    const { ESPLoader, Transport } = await import("https://unpkg.com/esptool-js@0.5.0/bundle.js");

    transport = new Transport(port, true);
    esploader = new ESPLoader({
      transport: transport,
      baudrate: baudRate,
      terminal: {
        clean() {},
        writeLine(str) { logCustom(str); },
        write(str) { logCustom(str); }
      }
    });

    logCustom("Syncing with ESP bootloader...");
    progressText.textContent = "Syncing with bootloader...";
    const chip = await esploader.main();
    logCustom(`Successfully connected to: ${chip}`);

    if (eraseAll) {
      logCustom("Erasing entire flash memory (this may take 15-30 seconds)...");
      progressText.textContent = "Erasing flash...";
      await esploader.eraseFlash();
      logCustom("Flash erase completed.");
    }

    logCustom(`Preparing binary at offset 0x${offset.toString(16)} (${offset})...`);
    progressText.textContent = "Flashing...";

    const binaryString = esploader.ui8ToBstr(new Uint8Array(customFileBuffer));

    await esploader.writeFlash({
      fileArray: [
        {
          data: binaryString,
          address: offset
        }
      ],
      flashSize: "keep",
      flashMode: "keep",
      flashFreq: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const pct = Math.floor((written / total) * 100);
        progressBar.style.width = `${pct}%`;
        progressPct.textContent = `${pct}%`;
        progressText.textContent = `Writing: ${written} / ${total} bytes`;
      }
    });

    progressBar.style.width = "100%";
    progressPct.textContent = "100%";
    progressText.textContent = "Flash Complete!";
    logCustom("Flashing finished successfully! Resetting device...");

    await esploader.hardReset();
    logCustom("Device reset signal sent. Your CrossPoint Reader is ready!");
  } catch (err) {
    console.error("Flash error:", err);
    logCustom(`ERROR: ${err.message || err}`);
    progressText.textContent = `Error: ${err.message || err}`;
  } finally {
    if (transport) {
      try {
        await transport.disconnect();
      } catch (e) {
        // ignore
      }
    }
    btnFlash.disabled = false;
  }
}

// ============================================================================
// 6. Live Serial Monitor
// ============================================================================
let monitorPort = null;
let monitorReader = null;
let isMonitoring = false;

function initSerialMonitor() {
  const btnConnect = document.getElementById("btnConnectMonitor");
  const btnReset = document.getElementById("btnResetDevice");
  const btnClear = document.getElementById("btnClearMonitor");
  const btnCopy = document.getElementById("btnCopyMonitor");
  const terminal = document.getElementById("monitorTerminal");
  const baudSelect = document.getElementById("monitorBaud");
  const autoScroll = document.getElementById("autoScrollCheckbox");

  if (btnClear) {
    btnClear.addEventListener("click", () => {
      terminal.textContent = "";
    });
  }

  if (btnCopy) {
    btnCopy.addEventListener("click", () => {
      navigator.clipboard.writeText(terminal.textContent).then(() => {
        alert("Serial logs copied to clipboard!");
      });
    });
  }

  if (btnReset) {
    btnReset.addEventListener("click", async () => {
      if (!monitorPort) return;
      try {
        // Toggle RTS / DTR to trigger ESP32 hardware reset
        await monitorPort.setSignals({ dataTerminalReady: false, requestToSend: true });
        await new Promise((r) => setTimeout(r, 100));
        await monitorPort.setSignals({ dataTerminalReady: true, requestToSend: false });
        await new Promise((r) => setTimeout(r, 100));
        await monitorPort.setSignals({ dataTerminalReady: false, requestToSend: false });
        terminal.textContent += "\n[MONITOR] === Hardware Reset Triggered ===\n";
      } catch (err) {
        console.error("Reset error:", err);
      }
    });
  }

  if (btnConnect) {
    btnConnect.addEventListener("click", async () => {
      if (isMonitoring) {
        await stopMonitor();
      } else {
        await startMonitor();
      }
    });
  }

  async function startMonitor() {
    if (!("serial" in navigator)) {
      alert("Web Serial is not supported in this browser.");
      return;
    }

    try {
      const baudRate = parseInt(baudSelect.value, 10);
      monitorPort = await navigator.serial.requestPort();
      await monitorPort.open({ baudRate });

      isMonitoring = true;
      btnConnect.textContent = "Disconnect Monitor";
      btnConnect.classList.replace("btn-primary", "btn-secondary");
      btnReset.disabled = false;
      terminal.textContent += `\n[MONITOR] Connected at ${baudRate} baud.\n`;

      const textDecoder = new TextDecoderStream();
      monitorPort.readable.pipeTo(textDecoder.writable);
      monitorReader = textDecoder.readable.getReader();

      while (isMonitoring) {
        const { value, done } = await monitorReader.read();
        if (done) break;
        if (value) {
          terminal.textContent += value;
          if (autoScroll && autoScroll.checked) {
            terminal.scrollTop = terminal.scrollHeight;
          }
        }
      }
    } catch (err) {
      if (err.name !== "AbortError" && err.name !== "NotFoundError") {
        console.error("Monitor error:", err);
        terminal.textContent += `\n[MONITOR ERROR] ${err.message || err}\n`;
      }
      await stopMonitor();
    }
  }

  async function stopMonitor() {
    isMonitoring = false;
    if (monitorReader) {
      try {
        await monitorReader.cancel();
      } catch (e) {}
      monitorReader = null;
    }
    if (monitorPort) {
      try {
        await monitorPort.close();
      } catch (e) {}
      monitorPort = null;
    }

    btnConnect.innerHTML = `
      <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2"><polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/></svg>
      Connect Monitor
    `;
    btnConnect.classList.replace("btn-secondary", "btn-primary");
    btnReset.disabled = true;
    terminal.textContent += "\n[MONITOR] Disconnected.\n";
  }
}
