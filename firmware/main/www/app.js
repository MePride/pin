// Pin E-ink Display Web Interface

let deviceStatus = null;

// Initialize the application
document.addEventListener('DOMContentLoaded', function() {
    getStatus();
    setInterval(getStatus, 30000); // Update status every 30 seconds
});

// Get device status
async function getStatus() {
    try {
        const response = await fetch('/api/status');
        if (response.ok) {
            deviceStatus = await response.json();
            updateUI();
            document.getElementById('status').textContent = 'Connected';
        } else {
            document.getElementById('status').textContent = 'Error';
        }
    } catch (error) {
        console.error('Error getting status:', error);
        document.getElementById('status').textContent = 'Disconnected';
    }
}

// Update UI with current status
function updateUI() {
    if (deviceStatus) {
        document.getElementById('battery').textContent = deviceStatus.battery_percentage;
        document.getElementById('firmware').textContent = deviceStatus.firmware_version;
    }
}

// Refresh display
async function refreshDisplay() {
    try {
        const response = await fetch('/api/display/refresh', {
            method: 'POST'
        });
        if (response.ok) {
            alert('Display refreshed');
        } else {
            alert('Failed to refresh display');
        }
    } catch (error) {
        console.error('Error refreshing display:', error);
        alert('Error refreshing display');
    }
}

// Clear display
async function clearDisplay() {
    try {
        const response = await fetch('/api/display/clear', {
            method: 'POST'
        });
        if (response.ok) {
            alert('Display cleared');
        } else {
            alert('Failed to clear display');
        }
    } catch (error) {
        console.error('Error clearing display:', error);
        alert('Error clearing display');
    }
}