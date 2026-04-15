/* F44AA Pulse Rifle - Web Interface JavaScript */
console.log('F44AA JavaScript loading');

var lastUserInteraction = 0;
var cameraStreamActive = false;
var testFiring = false;
var testFireInterval = null;
var testStatusInterval = null;

document.addEventListener('input', function() { lastUserInteraction = Date.now(); });
document.addEventListener('change', function() { lastUserInteraction = Date.now(); });
document.addEventListener('touchstart', function() { lastUserInteraction = Date.now(); });
document.addEventListener('touchend', function() { lastUserInteraction = Date.now(); });

/* ── Status polling ── */
function updateStatus() {
  if (Date.now() - lastUserInteraction < 2000) return;
  fetch('/status').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('ammo').innerText = d.ammo;
    var magStatus = document.getElementById('magazine_status');
    magStatus.innerText = d.magazineInserted ? 'LOADED' : 'UNLOADED';
    magStatus.className = d.magazineInserted ? 'magazine-loaded' : 'magazine-empty';
    if (d.testModeEnabled) { document.getElementById('testAmmo').innerText = d.ammo; updateAmmoDisplay(d.ammo, 400); }
    document.getElementById('audiomode_display').innerText = d.audiomode;
    document.getElementById('volume_display').innerText = d.volume;
    document.getElementById('bluetooth_status').innerText = d.bluetoothEnabled ? (d.bluetoothConnected ? 'Connected' : 'Enabled, Not Connected') : 'Disabled';
    if (document.getElementById('volume').value != d.volume) document.getElementById('volume').value = d.volume;
    var expectedAudioMode = (d.audiomode === 'Line Out') ? '1' : '0';
    if (document.getElementById('audiomode_select').value != expectedAudioMode) document.getElementById('audiomode_select').value = expectedAudioMode;
    var expectedCamDisplay = d.cameraDisplayEnabled ? '1' : '0';
    if (document.getElementById('camdisplay_select').value != expectedCamDisplay) document.getElementById('camdisplay_select').value = expectedCamDisplay;
    document.getElementById('wifiStatus').innerText = d.wifiStatus;
    document.getElementById('connectedSSID').innerText = d.connectedSSID;
    document.getElementById('currentIP').innerText = d.currentIP;
    document.getElementById('firmwareVersion').innerText = d.version;
    var fpEl = document.getElementById('firmwarePartition');
    if (fpEl) fpEl.innerText = d.partition;
    document.getElementById('torchStatus').innerText = d.torchEnabled ? 'ON' : 'OFF';
    document.getElementById('torchToggle').innerText = d.torchEnabled ? 'OFF' : 'ON';
    if (document.getElementById('redSlider') && typeof d.torchRed !== 'undefined') {
      document.getElementById('redSlider').value = d.torchRed;
      document.getElementById('redValue').innerText = d.torchRed;
      document.getElementById('greenSlider').value = d.torchGreen;
      document.getElementById('greenValue').innerText = d.torchGreen;
      document.getElementById('blueSlider').value = d.torchBlue;
      document.getElementById('blueValue').innerText = d.torchBlue;
    }
    var cs = document.getElementById('cameraStatus');
    if (cs) cs.innerText = d.cameraStatus;
    var tms = document.getElementById('testModeStatus');
    if (tms) {
      tms.innerText = d.testModeEnabled ? 'ENABLED' : 'DISABLED';
      document.getElementById('testModeToggle').innerText = d.testModeEnabled ? 'DISABLE TEST MODE' : 'ENABLE TEST MODE';
      var testControls = document.getElementById('testControls');
      if (d.testModeEnabled) { testControls.classList.remove('hidden'); testControls.classList.add('visible'); }
      else { testControls.classList.add('hidden'); testControls.classList.remove('visible'); }
    }
  }).catch(function(e) { console.log('Status update failed:', e); });
}

/* ── Bluetooth logs ── */
function updateBluetoothLogs() {
  var el = document.getElementById('bluetoothLogs');
  if (!el) return;
  fetch('/bluetooth-logs').then(function(r) { return r.text(); }).then(function(d) {
    var el2 = document.getElementById('bluetoothLogs');
    if (el2) el2.innerHTML = d;
  }).catch(function(e) { console.log('Bluetooth log update failed:', e); });
}
function clearBluetoothLogs() {
  if (confirm('Clear Bluetooth logs?')) {
    fetch('/bluetooth-logs/clear').then(function(r) { return r.text(); }).then(function() {
      document.getElementById('bluetoothLogs').innerHTML = 'Bluetooth logs cleared';
      updateBluetoothLogs();
    }).catch(function(e) { console.log('Clear Bluetooth logs failed:', e); });
  }
}

/* ── Torch ── */
function toggleTorch() {
  fetch('/torch-toggle').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('torchStatus').innerText = d.enabled ? 'ON' : 'OFF';
    document.getElementById('torchToggle').innerText = d.enabled ? 'OFF' : 'ON';
    setTimeout(updateStatus, 200);
  }).catch(function(e) { console.log('Torch toggle failed:', e); });
}
function setTorchColor(r, g, b) {
  fetch('/torch-color?r=' + r + '&g=' + g + '&b=' + b).then(function(response) { return response.json(); }).then(function() {
    document.getElementById('redSlider').value = r;
    document.getElementById('greenSlider').value = g;
    document.getElementById('blueSlider').value = b;
    document.getElementById('redValue').innerText = r;
    document.getElementById('greenValue').innerText = g;
    document.getElementById('blueValue').innerText = b;
    setTimeout(updateStatus, 200);
  }).catch(function(e) { console.log('Torch color failed:', e); });
}
function updateCustomColor() {
  document.getElementById('redValue').innerText = document.getElementById('redSlider').value;
  document.getElementById('greenValue').innerText = document.getElementById('greenSlider').value;
  document.getElementById('blueValue').innerText = document.getElementById('blueSlider').value;
}
function applyCustomColor() {
  setTorchColor(document.getElementById('redSlider').value, document.getElementById('greenSlider').value, document.getElementById('blueSlider').value);
}

/* ── WiFi ── */
function scanWiFi() {
  fetch('/wifi-scan').then(function(r) { return r.json(); }).then(function(d) {
    var l = document.getElementById('networkList');
    var html = '';
    for (var i = 0; i < d.networks.length; i++) {
      var n = d.networks[i];
      var lockIcon = n.secure ? '&#128274;' : '&#128275;';
      html += "<div class='network-item' onclick=\"selectNetwork('" + n.ssid.replace(/"/g, '&quot;') + "'," + (n.secure ? 1 : 0) + ")\">" + n.ssid + " (" + n.rssi + "dBm) " + lockIcon + " " + n.auth + "</div>";
    }
    l.innerHTML = html;
  }).catch(function(e) { console.log('WiFi scan failed:', e); });
}
function selectNetwork(ssid, secure) {
  document.getElementById('selectedSSID').innerText = ssid;
  var ps = document.getElementById('passwordSection');
  if (secure) { ps.classList.remove('hidden'); ps.classList.add('visible'); document.getElementById('wifiPassword').focus(); }
  else { ps.classList.add('hidden'); ps.classList.remove('visible'); document.getElementById('wifiPassword').value = ''; }
  document.getElementById('connectForm').classList.remove('hidden');
  document.getElementById('connectForm').classList.add('visible');
}
function connectToWiFi() {
  var ssid = document.getElementById('selectedSSID').innerText;
  var password = document.getElementById('wifiPassword').value;
  var params = new URLSearchParams();
  params.append('ssid', ssid);
  params.append('password', password);
  fetch('/wifi-connect', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: params }).then(function(r) { return r.text(); }).then(function(d) {
    alert('WiFi connection initiated: ' + d);
    document.getElementById('connectForm').classList.add('hidden');
    document.getElementById('connectForm').classList.remove('visible');
    setTimeout(updateStatus, 5000);
  }).catch(function(e) { alert('Connection failed: ' + e); });
}
function cancelConnect() {
  document.getElementById('connectForm').classList.add('hidden');
  document.getElementById('connectForm').classList.remove('visible');
  document.getElementById('wifiPassword').value = '';
}
function wifi_disconnect() {
  if (confirm('Disconnect from current WiFi network?')) {
    fetch('/wifi-disconnect').then(function(r) { return r.text(); }).then(function(d) { alert(d); setTimeout(updateStatus, 2000); }).catch(function(e) { alert('Disconnect failed: ' + e); });
  }
}
function forgetNetwork() {
  if (confirm('Forget saved WiFi network credentials?')) {
    fetch('/wifi-forget').then(function(r) { return r.text(); }).then(function(d) { alert(d); setTimeout(updateStatus, 2000); }).catch(function(e) { alert('Forget failed: ' + e); });
  }
}

/* ── OTA ── */
function uploadFirmware() {
  var fileInput = document.getElementById('firmwareFile');
  var file = fileInput.files[0];
  if (!file) { alert('Please select a firmware file (.bin)'); return; }
  if (!file.name || file.name.slice(-4).toLowerCase() !== '.bin') { alert('Please select a valid .bin firmware file'); return; }
  if (confirm('Upload firmware: ' + file.name + '? Device will restart after upload.')) {
    var formData = new FormData();
    formData.append('firmware', file);
    var otaProgress = document.getElementById('otaProgress');
    otaProgress.classList.remove('hidden');
    otaProgress.classList.add('visible');
    var xhr = new XMLHttpRequest();
    xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
        var percent = Math.round((e.loaded / e.total) * 100);
        document.getElementById('uploadPercent').innerText = percent;
        document.getElementById('progressBar').style.width = percent + '%';
      }
    };
    xhr.onload = function() {
      if (xhr.status === 200) {
        setTimeout(function() { alert('Firmware uploaded successfully. Device is restarting.'); window.location.reload(); }, 3000);
      } else { alert('Firmware upload failed: ' + xhr.responseText); }
    };
    xhr.onerror = function() { alert('Upload error occurred'); };
    xhr.open('POST', '/ota-upload');
    xhr.send(formData);
  }
}

/* ── Camera ── */
function toggleCameraStream() {
  var streamImg = document.getElementById('cameraStream');
  var placeholder = document.getElementById('cameraPlaceholder');
  var btn = document.getElementById('streamToggle');
  if (cameraStreamActive) {
    cameraStreamActive = false; streamImg.src = ''; streamImg.classList.add('hidden');
    placeholder.classList.remove('hidden'); btn.innerText = 'START STREAM';
    document.getElementById('cameraStatus').innerText = 'Stopped';
  } else {
    cameraStreamActive = true; streamImg.src = '/camera-stream?t=' + Date.now();
    streamImg.classList.remove('hidden'); placeholder.classList.add('hidden');
    btn.innerText = 'STOP STREAM'; document.getElementById('cameraStatus').innerText = 'Streaming';
  }
}
function capturePhoto() { window.open('/camera-capture?t=' + Date.now(), '_blank'); }

/* ── Config ── */
function updateConfigFallback() {
  var vol = document.getElementById('volume').value;
  var am = document.getElementById('audiomode_select').value;
  window.location.href = '/config-get?volume=' + vol + '&audiomode=' + am;
}

/* ── Display color ── */
function setDisplayColor() {
  var colorValue = String(document.getElementById('colorInput').value || '').toUpperCase();
  if (!/^[0-9A-F]{1,4}$/.test(colorValue)) { alert('Please enter a valid hex color (e.g., F800 for red)'); return; }
  var paddedColor = ('0000' + colorValue).slice(-4);
  fetch('/display-color?color=' + paddedColor).then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('currentColor').innerText = d.color;
    document.getElementById('colorDisplay').innerText = d.color;
    document.getElementById('colorSlider').value = parseInt(paddedColor, 16);
  }).catch(function(e) { alert('Failed to set color: ' + e); });
}
function updateColorFromSlider(value) {
  var v = parseInt(value, 10) || 0;
  var hexValue = ('0000' + v.toString(16).toUpperCase()).slice(-4);
  document.getElementById('colorDisplay').innerText = '0x' + hexValue;
  document.getElementById('colorInput').value = hexValue;
  fetch('/display-color?color=' + hexValue).then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('currentColor').innerText = d.color;
  }).catch(function(e) { console.log('Slider color update failed:', e); });
}
function setPresetColor(hexColor) { document.getElementById('colorInput').value = hexColor; setDisplayColor(); }

/* ── Ammo display (ASCII art for test mode) ── */
function updateAmmoDisplay(current, max) {
  current = parseInt(current, 10) || 0;
  max = parseInt(max, 10) || 400;
  var bars = 0;
  if (current >= 336) bars = 6; else if (current >= 269) bars = 5;
  else if (current >= 202) bars = 4; else if (current >= 135) bars = 3;
  else if (current >= 68) bars = 2; else if (current >= 1) bars = 1;
  var display = '+--------------------------------------+\n|                                      |\n';
  for (var i = 0; i < 6; i++) {
    display += '|';
    var barLine = '';
    for (var j = 0; j < 6; j++) { barLine += (j < bars) ? '####' : '    '; if (j < 5) barLine += '  '; }
    while (barLine.length < 38) barLine += ' ';
    display += barLine + '|\n';
  }
  display += '|                                      |\n+--------------------------------------+\nAMMO: ' + current + '/' + max + ' rounds (' + bars + ' bars)';
  var el = document.getElementById('ammoDisplay');
  if (el) {
    el.innerText = display;
    el.className = 'ammo-display';
    if (current === 0) el.classList.add('ammo-empty');
    else if (current < max * 0.2) el.classList.add('ammo-low');
    else el.classList.add('ammo-normal');
  }
}

/* ── Test mode ── */
function toggleTestMode() {
  fetch('/test-mode/toggle').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('testModeStatus').innerText = d.enabled ? 'ENABLED' : 'DISABLED';
    document.getElementById('testModeToggle').innerText = d.enabled ? 'DISABLE TEST MODE' : 'ENABLE TEST MODE';
    var tc = document.getElementById('testControls');
    if (d.enabled) { tc.classList.remove('hidden'); tc.classList.add('visible'); document.getElementById('testAmmo').innerText = (d.ammo || 400); updateAmmoDisplay((d.ammo || 400), 400); }
    else { tc.classList.add('hidden'); tc.classList.remove('visible'); updateAmmoDisplay(0, 400); }
    setTimeout(updateStatus, 200);
  }).catch(function(e) { console.log('Test mode toggle failed:', e); });
}
function updateTestAmmo() {
  if (testFiring) {
    fetch('/status').then(function(r) { return r.json(); }).then(function(d) {
      if (d.testModeEnabled) { document.getElementById('testAmmo').innerText = d.ammo; updateAmmoDisplay(d.ammo, 400); }
    }).catch(function(e) { console.log('Test ammo update failed:', e); });
  }
}
function toggleTestFire() {
  if (!testFiring) {
    testFiring = true;
    var btn = document.getElementById('testFireButton');
    btn.innerText = 'STOP FULL AUTO'; btn.classList.add('test-fire-active');
    testFireInterval = setInterval(updateTestAmmo, 500);
    testStatusInterval = setInterval(updateStatus, 1000);
    fetch('/test-mode/fire-start').then(function(r) { return r.json(); }).then(function() { setTimeout(updateStatus, 200); }).catch(function(e) {
      console.log('Test fire start failed:', e);
      testFiring = false; var b = document.getElementById('testFireButton'); b.innerText = 'START FULL AUTO'; b.classList.remove('test-fire-active');
      if (testFireInterval) { clearInterval(testFireInterval); testFireInterval = null; }
      if (testStatusInterval) { clearInterval(testStatusInterval); testStatusInterval = null; }
    });
  } else {
    testFiring = false;
    if (testFireInterval) { clearInterval(testFireInterval); testFireInterval = null; }
    if (testStatusInterval) { clearInterval(testStatusInterval); testStatusInterval = null; }
    var btn = document.getElementById('testFireButton');
    btn.innerText = 'START FULL AUTO'; btn.classList.remove('test-fire-active');
    fetch('/test-mode/fire-stop').then(function(r) { return r.json(); }).then(function(d) {
      document.getElementById('testAmmo').innerText = (d.ammo || 0);
      updateAmmoDisplay((d.ammo || 0), 400);
      setTimeout(updateStatus, 200);
    }).catch(function(e) { console.log('Test fire stop failed:', e); });
  }
}
function updateAmmoDisplayPeriodic() {
  var tme = document.getElementById('testModeStatus');
  if (tme && tme.innerText === 'ENABLED') {
    var currentAmmo = parseInt(document.getElementById('testAmmo').innerText, 10) || 0;
    updateAmmoDisplay(currentAmmo, 400);
  }
}

/* ── Init ── */
window.addEventListener('DOMContentLoaded', function() {
  setInterval(updateStatus, 5000);
  setTimeout(updateStatus, 1000);
  setInterval(updateAmmoDisplayPeriodic, 500);
  setTimeout(function() { updateAmmoDisplay(400, 400); }, 500);
});
