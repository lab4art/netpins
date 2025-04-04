function showLoader() {
    const glassPane = document.getElementById('glass-pane');
    glassPane.style.display = 'flex';
}

function hideLoader() {
    const glassPane = document.getElementById('glass-pane');
    glassPane.style.display = 'none';
}

const NotificationType = {
    ERROR: 'error',
    SUCCESS: 'success'
}

function showNotificationPanel(message, type, hideAfter) {
    document.getElementById('notification-message').innerText = message;
    var panel = document.getElementById('notification-panel');
    if (type === NotificationType.ERROR) {
        panel.className = 'notification-panel notification-panel-error';
    } else {
        panel.className = 'notification-panel';
    }
    panel.style.display = 'block';
    if (hideAfter) {
        setTimeout(() => {
            hideNotificationPanel();
        }, hideAfter);
    }
}

function hideNotificationPanel() {
    var panel = document.getElementById('notification-panel');
    panel.style.display = 'none';
}

function fetchAndUpdateDMX() {
    fetch('/conf/dmx')
    .then(response => response.json())
    .then(data => {
        document.getElementById('universe').value = data.universe;
        document.getElementById('channel').value = data.channel;
    });
}

function fetchAndUpdateSysInfo() {
    fetch('/sys-info')
    .then(response => response.json())
    .then(data => {
        document.getElementById('hostname').innerText = data.hostname;
        document.getElementById('firmware').innerText = data.firmware;
        // update page title
        if (data.hostname) {
            document.title = 'NetPins: ' + data.hostname;
        } else {
            document.title = 'NetPins';
        }
    });
}

function fetchAndUpdateSettings() {
    fetch('/conf/sys')
    .then(response => response.json())
    .then(data => {
        document.getElementById('settings').value = jsyaml.dump(data);
    });
}

document.addEventListener("DOMContentLoaded", function() {
    fetchAndUpdateSysInfo();
    fetchAndUpdateDMX();
    fetchAndUpdateSettings();
});

function processResponse(data) {
    if (data.status === 'OK' || data.status === 'OK_REBOOT') {
        if (data.reloadDelay >= 0) {
            setTimeout(() => {
                window.location.hash = '';
                window.location.reload();
            }, data.reloadDelay);
            showNotificationPanel(data.message, NotificationType.SUCCESS);
        } else {
            fetchAndUpdateSysInfo();
            fetchAndUpdateDMX();
            fetchAndUpdateSettings();
            hideLoader();
            showNotificationPanel(data.message, NotificationType.SUCCESS, 3000);
        }
    } else {
        if (data.reloadDelay >= 0) {
            showNotificationPanel(data.message, NotificationType.ERROR);
            setTimeout(() => {
                window.location.reload();
            }, data.reloadDelay);
        } else {
            showNotificationPanel(data.message, NotificationType.ERROR);
            hideLoader();
        }
    }
}

function postYamlAsJson() {
    const settingsEl = document.getElementById('settings');
    try {
        const settingsJson = jsyaml.load(settingsEl.value);

        const json = {
            "command": "sys-config",
            "data": settingsJson
        };

        showLoader();
        fetch('/system', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(json)
        })
        .then(response => response.json())
        .then(data => {
            console.log('Response:', data);
            processResponse(data);
        })
        .catch(error => {
            hideLoader();
            showNotificationPanel(error.message, NotificationType.ERROR);
        });

    } catch (error) {
        hideLoader();
        showNotificationPanel('Error parsing YAML: ' + error.message, NotificationType.ERROR);
    }
}

function postFormAsJson(formId) {
    const formEl = document.getElementById(formId);
    const formData = new FormData(formEl);

    const data = {};
    formData.forEach((value, key) => {
        data[key] = value;
    });

    postJson(formId, data);
}

function postJson(command, data) {
    const json = {
        "command": command,
        "data": data
    };
    
    showLoader();
    fetch('/system', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(json)
    })
    .then(response => response.json())
    .then(data => {
        console.log('Response:', data);
        processResponse(data);
    })
    .catch(error => {
        hideLoader();
        showNotificationPanel(error.message, NotificationType.ERROR);
    });
}
