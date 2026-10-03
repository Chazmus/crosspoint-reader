/**
 * CrossPoint Web Flasher — Application Logic & Web Serial Integration
 * Smart USB Filtering & Direct esptool-js Flashing Engine
 */

// USB Vendor & Product IDs for Espressif and E-Reader USB hardware.
// Filtering against this list hides motherboard /dev/ttyS* ports on Linux.
const USB_FILTERS = [
  // Espressif USB-JTAG/CDC (ESP32-S3 built-in USB)
  { usbVendorId: 0x303a, usbProductId: 0x1001 },
  // Espressif USB-Serial/OTG
  { usbVendorId: 0x303a, usbProductId: 0x1002 },
  // All Espressif devices (VID 0x303A / 12346)
  { usbVendorId: 0x303a },
  // Seeed Technology Co., Ltd. (VID 0x2886 / 10374) — Seeed reTerminal Sticky
  { usbVendorId: 0x2886 },
  // Silicon Labs CP210x (VID 0x10C4 / 4292)
  { usbVendorId: 0x10c4 },
  // WCH QinHeng CH340 / CH341 / CH9102 (VID 0x1A86 / 6790)
  { usbVendorId: 0x1a86 },
  // FTDI (VID 0x0403 / 1027)
  { usbVendorId: 0x0403 }
];

function getPortFilters() {
  const showAll = document.getElementById("chkShowAllPorts")?.checked;
  if (showAll) {
    return {};
  }
  return { filters: USB_FILTERS };
}

// Current selection state
let currentDevice = {
  id: "x4pro",
  name: "Xteink X4Pro",
  chip: "ESP32-S3",
  file: "firmware-x4pro.bin",
  manifest: "manifests/x4pro.json"
};

// ============================================================================
// 1. Initialization
// ============================================================================
document.addEventListener("DOMContentLoaded", () => {
  initTabs();
  initDeviceSelector();
  initBrowserCheck();
  initPortAutoDetect();
  loadMetadata();
  initWebInstaller();
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
// 2. Browser Compatibility & USB Device Auto-Detection
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

async function initPortAutoDetect() {
  if (!("serial" in navigator)) return;

  const updateBanner = async () => {
    try {
      const ports = await navigator.serial.getPorts();
      const banner = document.getElementById("detectedPortBanner");
      const text = document.getElementById("detectedPortText");

      if (ports.length > 0) {
        const info = ports[0].getInfo();
        const vid = info.usbVendorId ? "0x" + info.usbVendorId.toString(16).toUpperCase() : "";
        const pid = info.usbProductId ? "0x" + info.usbProductId.toString(16).toUpperCase() : "";

        let label = "Paired USB device detected";
        if (info.usbVendorId === 0x303a) {
          label = `Connected: Espressif ESP32-S3 (${vid}:${pid})`;
        } else if (info.usbVendorId === 0x2886) {
          label = `Connected: Seeed Device (${vid}:${pid})`;
        } else if (vid) {
          label = `Connected USB Device: VID ${vid} PID ${pid}`;
        }

        text.textContent = label;
        banner.style.display = "flex";
      } else {
        banner.style.display = "none";
      }
    } catch (e) {
      console.debug("getPorts error:", e);
    }
  };

  await updateBanner();
  navigator.serial.addEventListener("connect", updateBanner);
  navigator.serial.addEventListener("disconnect", updateBanner);
}

// ============================================================================
// 3. Device Selector
// ============================================================================
function initDeviceSelector() {
  const cards = document.querySelectorAll(".device-card");
  const targetName = document.getElementById("targetName");
  const targetChip = document.getElementById("targetChip");
  const targetFile = document.getElementById("targetFile");
  const btnDownloadBin = document.getElementById("btnDownloadBin");

  cards.forEach((card) => {
    card.addEventListener("click", () => {
      cards.forEach((c) => c.classList.remove("selected"));
      card.classList.add("selected");

      currentDevice.id = card.getAttribute("data-device");
      currentDevice.name = card.getAttribute("data-name");
      currentDevice.chip = card.getAttribute("data-chip");
      currentDevice.file = card.getAttribute("data-file");
      currentDevice.manifest = card.getAttribute("data-manifest");

      if (targetName) targetName.textContent = currentDevice.name;
      if (targetChip) targetChip.textContent = currentDevice.chip;
      if (targetFile) targetFile.textContent = currentDevice.file;

      if (btnDownloadBin) {
        btnDownloadBin.href = `./firmware/${currentDevice.file}`;
        btnDownloadBin.setAttribute("download", currentDevice.file);
      }
    });
  });
}

// ============================================================================
// 4. Metadata Loader
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
// 5. Native Smart-Filtered Web Installer
// ============================================================================
let activeInstallTransport = null;
let activeInstallLoader = null;
let isInstalling = false;

function initWebInstaller() {
  const btnStart = document.getElementById("btnStartInstall");
  const modal = document.getElementById("installModal");
  const modalClose = document.getElementById("modalCloseBtn");
  const modalAction = document.getElementById("modalActionBtn");

  if (modalClose) {
    modalClose.addEventListener("click", () => {
      if (!isInstalling) {
        modal.style.display = "none";
      }
    });
  }

  if (modalAction) {
    modalAction.addEventListener("click", async () => {
      if (isInstalling) {
        if (confirm("Flashing is in progress. Are you sure you want to cancel?")) {
          isInstalling = false;
          if (activeInstallTransport) {
            try { await activeInstallTransport.disconnect(); } catch (e) {}
          }
          modal.style.display = "none";
        }
      } else {
        modal.style.display = "none";
      }
    });
  }

  if (btnStart) {
    btnStart.addEventListener("click", async () => {
      if (!("serial" in navigator)) {
        alert("Web Serial is not supported in this browser. Please use Chrome, Edge, Brave, or Opera.");
        return;
      }
      await runSmartWebInstall();
    });
  }
}

function updateModalStep(stepNum) {
  for (let i = 1; i <= 4; i++) {
    const el = document.getElementById(`step${i}`);
    if (!el) continue;
    el.classList.remove("active", "done");
    if (i < stepNum) {
      el.classList.add("done");
    } else if (i === stepNum) {
      el.classList.add("active");
    }
  }
}

function logModal(msg) {
  const terminal = document.getElementById("modalLogTerminal");
  if (!terminal) return;
  const time = new Date().toLocaleTimeString();
  terminal.textContent += `[${time}] ${msg}\n`;
  terminal.scrollTop = terminal.scrollHeight;
}

async function runSmartWebInstall() {
  const modal = document.getElementById("installModal");
  const modalTargetSubtitle = document.getElementById("modalTargetSubtitle");
  const modalDeviceCard = document.getElementById("modalDeviceCard");
  const modalChipBadge = document.getElementById("modalChipBadge");
  const modalChipDesc = document.getElementById("modalChipDesc");
  const modalMac = document.getElementById("modalMac");
  const modalProgressBar = document.getElementById("modalProgressBar");
  const modalStatusText = document.getElementById("modalStatusText");
  const modalStatusPct = document.getElementById("modalStatusPct");
  const modalLogTerminal = document.getElementById("modalLogTerminal");
  const modalActionBtn = document.getElementById("modalActionBtn");

  // Reset UI
  modalTargetSubtitle.textContent = `${currentDevice.name} (${currentDevice.chip})`;
  modalDeviceCard.style.display = "none";
  modalProgressBar.style.width = "0%";
  modalStatusPct.textContent = "0%";
  modalStatusText.textContent = "Opening serial port picker...";
  modalLogTerminal.textContent = "";
  modalActionBtn.textContent = "Cancel";
  modalActionBtn.classList.replace("btn-primary", "btn-secondary");

  let port = null;
  try {
    // PASS USB_FILTERS: This hides all 32 /dev/ttyS* ports on Linux!
    port = await navigator.serial.requestPort(getPortFilters());
  } catch (err) {
    if (err.name === "NotFoundError") {
      // User cancelled port picker
      return;
    }
    alert(`Could not select serial port: ${err.message || err}`);
    return;
  }

  modal.style.display = "flex";
  isInstalling = true;
  updateModalStep(1); // Connect

  try {
    logModal(`Connecting to device... Port selected.`);
    modalStatusText.textContent = "Loading esptool-js flashing engine...";

    const { ESPLoader, Transport } = await import("https://unpkg.com/esptool-js@0.5.0/bundle.js");

    activeInstallTransport = new Transport(port, true);
    activeInstallLoader = new ESPLoader({
      transport: activeInstallTransport,
      baudrate: 921600,
      romBaudrate: 115200,
      terminal: {
        clean() {},
        writeLine(str) { logModal(str); },
        write(str) { logModal(str); }
      }
    });

    updateModalStep(2); // Handshake
    modalStatusText.textContent = "Syncing with ESP32-S3 bootloader...";
    logModal("Establishing communication with ROM bootloader...");

    const chipDesc = await activeInstallLoader.main();
    const chipName = activeInstallLoader.chip ? activeInstallLoader.chip.CHIP_NAME : "ESP32-S3";
    let macAddress = "--:--:--";
    try {
      macAddress = await activeInstallLoader.chip.readMac(activeInstallLoader);
    } catch (e) {}

    logModal(`Identified chip: ${chipName} — ${chipDesc}`);
    logModal(`MAC Address: ${macAddress}`);

    // Update Device Info Card
    modalDeviceCard.style.display = "block";
    modalChipBadge.textContent = chipName;
    modalChipDesc.textContent = chipDesc || chipName;
    modalMac.textContent = macAddress;

    // Verify it's an S3 device
    if (!chipName.toUpperCase().includes("S3")) {
      const proceed = confirm(
        `Warning: Detected chip is '${chipName}', but this firmware branch is designed for ESP32-S3 devices.\n\nDo you want to continue flashing anyway?`
      );
      if (!proceed) {
        throw new Error("Flashing cancelled: chip model does not match ESP32-S3.");
      }
    }

    updateModalStep(3); // Flash
    modalStatusText.textContent = `Downloading ${currentDevice.file}...`;
    logModal(`Fetching prebuilt firmware from server: ./firmware/${currentDevice.file}`);

    const fwRes = await fetch(`./firmware/${currentDevice.file}?t=${Date.now()}`);
    if (!fwRes.ok) {
      throw new Error(`Firmware file './firmware/${currentDevice.file}' could not be loaded from server (HTTP ${fwRes.status}). Ensure the GitHub Actions build completed successfully.`);
    }

    const fwBuffer = await fwRes.arrayBuffer();
    logModal(`Firmware downloaded: ${fwBuffer.byteLength} bytes.`);

    modalStatusText.textContent = "Writing firmware to flash partition (0x10000)...";
    const binaryString = activeInstallLoader.ui8ToBstr(new Uint8Array(fwBuffer));

    await activeInstallLoader.writeFlash({
      fileArray: [
        {
          data: binaryString,
          address: 0x10000 // CrossPoint app0 partition
        }
      ],
      flashSize: "keep",
      flashMode: "keep",
      flashFreq: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        if (!isInstalling) return;
        const pct = Math.floor((written / total) * 100);
        modalProgressBar.style.width = `${pct}%`;
        modalStatusPct.textContent = `${pct}%`;
        const writtenMb = (written / (1024 * 1024)).toFixed(2);
        const totalMb = (total / (1024 * 1024)).toFixed(2);
        modalStatusText.textContent = `Writing: ${writtenMb} MB / ${totalMb} MB`;
      }
    });

    updateModalStep(4); // Reboot
    modalProgressBar.style.width = "100%";
    modalStatusPct.textContent = "100%";
    modalStatusText.textContent = "Flashing complete! Resetting device...";
    logModal("Firmware successfully written! Sending hardware reset signal...");

    await activeInstallLoader.hardReset();
    logModal("Device rebooted into CrossPoint Reader.");

    modalStatusText.textContent = "Success! CrossPoint is rebooting.";
    modalActionBtn.textContent = "Done";
    modalActionBtn.classList.replace("btn-secondary", "btn-primary");
  } catch (err) {
    console.error("Installation error:", err);
    logModal(`ERROR: ${err.message || err}`);
    modalStatusText.textContent = `Error: ${err.message || err}`;
    modalActionBtn.textContent = "Close";
  } finally {
    isInstalling = false;
    if (activeInstallTransport) {
      try {
        await activeInstallTransport.disconnect();
      } catch (e) {}
      activeInstallTransport = null;
    }
  }
}

// ============================================================================
// 6. Custom .bin Flasher (uses same smart USB filter)
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
        alert("Web Serial is not supported in this browser. Please use Chrome, Edge, or Brave.");
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
    logCustom("Requesting Serial Port (smart filtered)...");
    const port = await navigator.serial.requestPort(getPortFilters());

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
    if (err.name === "NotFoundError") {
      logCustom("User cancelled port selection.");
    } else {
      console.error("Flash error:", err);
      logCustom(`ERROR: ${err.message || err}`);
      progressText.textContent = `Error: ${err.message || err}`;
    }
  } finally {
    if (transport) {
      try {
        await transport.disconnect();
      } catch (e) {}
    }
    btnFlash.disabled = false;
  }
}

// ============================================================================
// 7. Live Serial Monitor (uses same smart USB filter)
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
      // Use getPortFilters() here too!
      monitorPort = await navigator.serial.requestPort(getPortFilters());
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
