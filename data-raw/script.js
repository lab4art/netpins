document.addEventListener("DOMContentLoaded", function() {
    fetch('/sys-info')
        .then(response => response.json())
        .then(data => {
            document.getElementById('hostname').innerText = data.hostname;
            document.getElementById('firmware').innerText = data.firmware;
        });
    fetch('/conf/sys')
        .then(response => response.json())
        .then(data => {
            document.getElementById('settings').value = jsyaml.dump(data);
        });
});

function renderForm(formContainerId, formDef, data) {
    const formContainer = document.getElementById(formContainerId);
    const form = document.createElement('form');
    form.onsubmit = function (event) {
        event.preventDefault();
        const formData = {
            'command': '',
            'data': {}
        };
        formData['command'] = formDef.command;
        formDef.fields.forEach(field => {
            formData['data'][field.name] = form.elements[field.name].value;
        });
        // console.log(formData);
        // post json data to server
        fetch('/system', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(formData),
            })
            .catch((error) => {
                // console.error('Error:', error);
            });
    };

    formDef.fields.forEach(field => {
        const fieldWrapper = document.createElement('div');
        fieldWrapper.className = 'form-group';

        if (field.label) {
            const label = document.createElement('label');
            label.textContent = field.label;
            label.setAttribute('for', field.name);
            fieldWrapper.appendChild(label);
        
            if (field.tooltip) {
                const tooltip = document.createElement('span');
                tooltip.className = 'tooltip';
                tooltip.textContent = '?';
    
                const tooltipText = document.createElement('span');
                tooltipText.className = 'tooltiptext';
                tooltipText.innerHTML = field.tooltip;
    
                tooltip.appendChild(tooltipText);
                label.appendChild(tooltip);
            }
        }

        const input = document.createElement('input');
        input.type = field.type;
        input.name = field.name;
        if (field.name in data) {
            input.value = data[field.name];
        }
        fieldWrapper.appendChild(input);
        form.appendChild(fieldWrapper);
    });
    submitEl = document.createElement('input');
    submitEl.type = 'submit';
    submitEl.value = formDef.submitLabel;
    form.appendChild(submitEl);
    formContainer.appendChild(form);
}

function postYamlAsJson() {
    // const form = document.getElementById('sys-config-form');
    // const formData = new FormData(form);
    // const yaml = formData.get('settings');
    // const json = jsyaml.load(yaml);
    const settingsEl = document.getElementById('settings');
    try {
        const settingsJson = jsyaml.load(settingsEl.value);

        const json = {
            "command": "sys-config",
            "data": settingsJson
        };

        fetch('/system', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(json)
        })
        // .then(response => {
        //     if (!response.ok) {
        //         throw new Error('Network response was not ok');
        //     }
        //     return response.json();
        // })
        .then(data => {
            console.log('Success:', data);
        })
        // .catch(error => {
        //     alert('Error during fetch: ' + error.message);
        // })
        ;

    } catch (error) {
        alert('Error parsing YAML: ' + error.message);
    }
}
