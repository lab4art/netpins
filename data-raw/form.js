function renderForm(formContainerId, formDef, data) {
    const formContainer = document.getElementById(formContainerId);
    const form = document.createElement('form');
    form.onsubmit = function (event) {
        event.preventDefault();
        const formData = {};
        formData['command'] = formDef.command;
        formDef.fields.forEach(field => {
            formData[field.name] = form.elements[field.name].value;
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

