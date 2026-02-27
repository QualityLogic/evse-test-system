/**
 * @typedef {object} ScenarioCreateEvent
 * @property {string} id
 * @property {object} scenario
 * @property {string} scenario.id
 * @property {string} scenario.display_name
 * @property {object} protocol
 * @property {string} protocol.id
 * @property {string} protocol.display_name
 * @property {object} charge_mode
 * @property {string} charge_mode.id
 * @property {string} charge_mode.display_name
 * @property {object} ident_mode
 * @property {string} ident_mode.id
 * @property {string} ident_mode.display_name
 * @property {string} created_at
 * @property {string} status
 * @property {string?} outcome
 */

/**
 * @type {Record<string, TrackedScenario>}
 */
const TrackedScenarios = {};
const THEME_STORAGE_KEY = 'color-theme';

/**
 * Apply a color theme to the DOM and save it to local storage.
 * @param {"light"|"dark"} theme The color theme to apply.
 * @param {boolean} [save=false] Whether to save the theme to localstorage.
 */
function applyColorTheme(theme, save) {
    if (theme === 'light') {
        if (document.documentElement.getAttribute('data-bs-theme') !== 'light') {
            document.documentElement.setAttribute('data-bs-theme', 'light');
            document.getElementById('theme-moon-icon').removeAttribute('hidden');
            document.getElementById('theme-sun-icon').setAttribute('hidden', '');
        }
        if (save) {
            localStorage.setItem(THEME_STORAGE_KEY, theme);
        }
    }
    else if (theme === 'dark') {
        if (document.documentElement.getAttribute('data-bs-theme') !== 'dark') {
            document.documentElement.setAttribute('data-bs-theme', 'dark');
            document.getElementById('theme-moon-icon').setAttribute('hidden', '');
            document.getElementById('theme-sun-icon').removeAttribute('hidden');
        }
        if (save) {
            localStorage.setItem(THEME_STORAGE_KEY, theme);
        }
    } else {
        console.error(`Ignoring unknown color theme: ${theme}`);
    }
}

/**
 * Load a color theme from localstorage and apply it to the DOM.
 */
function loadColorTheme() {
    const savedColorTheme = localStorage.getItem(THEME_STORAGE_KEY);
    if (savedColorTheme) {
        applyColorTheme(savedColorTheme, false);
    }
}

class TrackedScenario {
    #htmlElement;
    #scenarioColor;
    #statusColor;
    #statusElement;

    /**
     * Create a new instance of the class.
     * @param {ScenarioCreateEvent} data Scenario creation data from the API.
     */
    constructor(data) {
        this.results = data['result'];
        this.#buildHtmlElements(data);
        const status = getStatusDisplayName(data.status, data.outcome);
        this.setStatusColor(getStatusColor(status));
        this.setScenarioColor(getScenarioColor(status));
    }

    /**
     * Get the root HTML Element of the scenario.
     * @returns {HTMLElement}
     */
    element() {
        return this.#htmlElement;
    }

    /**
     * Update the displayed status text.
     * @param {string} status The new status text.
     */
    setStatusText(status) {
        if (this.#statusElement !== undefined) {
            if (this.#statusElement.innerText !== status) {
                this.#statusElement.innerText = status;
            }
        }
    }

    /**
     * Update the color of the displayed status badge.
     * @param {"primary"|"secondary"|"danger"|"success"|"warning"} color The new status color.
     */
    setStatusColor(color) {
        if (this.#statusElement !== undefined && this.#statusColor !== color) {
            const newColorClass = `text-bg-${color}`;
            if (this.#statusColor !== undefined) {
                const oldColorClass = `text-bg-${this.#statusColor}`;
                this.#statusElement.classList.replace(oldColorClass, newColorClass);
            } else {
                this.#statusElement.classList.add(newColorClass);
            }
            this.#statusColor = color;
        }
    }

    /**
     * Update the color of the scenario background.
     * @param {"primary"|"secondary"|"danger"|"success"|"warning"|undefined} color The new scenario color.
     */
    setScenarioColor(color) {
        if (this.#htmlElement !== undefined && this.#scenarioColor !== color) {
            const newColorClass = `list-group-item-${color}`;
            if (this.#scenarioColor !== undefined) {
                const oldColorClass = `list-group-item-${this.#scenarioColor}`;
                if (color !== undefined) {
                    this.#htmlElement.classList.replace(oldColorClass, newColorClass);
                } else {
                    this.#htmlElement.classList.remove(oldColorClass);
                }
            } else if (color !== undefined) {
                this.#htmlElement.classList.add(newColorClass);
            }
            this.#scenarioColor = color;
        }
    }

    // ------------------------------------------
    // HTML Element Creation

    /**
     * Create the HTML Elements that represent the tracked scenario.
     * @param {ScenarioCreateEvent} data Scenario data from the API.
     */
    #buildHtmlElements(data) {
        const container = document.createElement('li');
        const header = this.#buildHeader(data);

        container.classList.add('list-group-item', 'list-group-item-action');
        container.setAttribute('data-bs-toggle', 'modal');
        container.setAttribute('data-bs-target', '#scenario-result-modal');
        container.setAttribute('data-bs-scenarioId', data.id);

        container.appendChild(header);

        this.#htmlElement = container;
    }

    /**
     * Create the HTML Elements for the test instance header.
     * @param {ScenarioCreateEvent} data Scenario data from the API.
     * @returns {HTMLDivElement} The created header HTML Element.
     */
    #buildHeader(data) {
        const headerDiv = document.createElement('div');
        const leftContainer = document.createElement('div');
        const rightContainer = document.createElement('div');
        const titleH5 = document.createElement('h5');
        const titleBadgeDiv = document.createElement('div');
        const protocolBadge = document.createElement('span');
        const chargeModeBadge = document.createElement('span');
        const identModeBadge = document.createElement('span');
        const statusBadge = document.createElement('span');

        titleH5.innerText = data.scenario.name;
        protocolBadge.innerText = data.protocol.name;
        chargeModeBadge.innerText = data.charge_mode.name;
        identModeBadge.innerText = data.ident_mode.name;
        statusBadge.innerText = getStatusDisplayName(data.status, data.outcome);

        headerDiv.classList.add('d-flex', 'w-100', 'justify-content-between');
        leftContainer.classList.add('d-flex', 'gap-2');
        titleH5.classList.add('mb-1');
        titleBadgeDiv.classList.add('ms-2');
        protocolBadge.classList.add('ms-1', 'badge', 'rounded-pill', 'text-bg-secondary');
        chargeModeBadge.classList.add('ms-1', 'badge', 'rounded-pill', 'text-bg-secondary');
        identModeBadge.classList.add('ms-1', 'badge', 'rounded-pill', 'text-bg-secondary');
        statusBadge.classList.add('badge', 'text-bg-secondary');

        titleBadgeDiv.appendChild(protocolBadge);
        titleBadgeDiv.appendChild(chargeModeBadge);
        titleBadgeDiv.appendChild(identModeBadge);
        leftContainer.appendChild(titleH5);
        leftContainer.appendChild(titleBadgeDiv);
        rightContainer.appendChild(statusBadge);
        headerDiv.appendChild(leftContainer);
        headerDiv.appendChild(rightContainer);

        this.#statusColor = 'secondary';
        this.#statusElement = statusBadge;

        return headerDiv;
    }
}

/**
 * Get the displayed status name for a tracked status.
 * @param {string} status Original scenario status.
 * @param {string} [outcome] Optional scenario outcome.
 * @returns {string} The display name for the status.
 */
function getStatusDisplayName(status, outcome) {
    let statusText = status.toUpperCase();
    let outcomeText = outcome?.toUpperCase();

    if (outcomeText !== undefined) {
        switch (outcomeText) {
            case 'NOTSTARTED':
                return 'N/A';
            case 'PASSCRITERIAMET':
                return 'PASS';
            case 'PASSCRITERIANOTMET':
                return 'FAIL';
            case 'PRECONDITIONSNOTMET':
                return 'INCOMPLETE';
            default:
                return 'UNKNOWN';
        }
    }

    if (statusText === 'QUEUED')
        statusText = 'PENDING';

    return statusText;
}

function getStatusColor(status) {
    switch (status) {
        case 'PASS':
            return 'success';
        case 'FAIL':
            return 'danger';
        case 'INCOMPLETE':
            return 'warning';
        case 'RUNNING':
            return 'primary';
        default:
            return 'secondary';
    }
}

function getScenarioColor(status) {
    switch (status) {
        case 'RUNNING':
            return 'primary';
        case 'PASS':
            return 'success';
        case 'FAIL':
            return 'danger';
        case 'INCOMPLETE':
        case 'UNKNOWN':
            return 'warning';
        default:
            return undefined;
    }
}

function handleScenarioCreatedEvent(event) {
    const scenario = new TrackedScenario(event);
    const scenarios = document.getElementById('tracked-scenarios');
    if (scenarios) {
        scenarios.appendChild(scenario.element());
    }
    TrackedScenarios[event.id] = scenario;
}

function handleScenarioDeletedEvent(event) {
    const scenario = TrackedScenarios[event.id];
    if (scenario) {
        scenario.element().remove();
        delete TrackedScenarios[event.id];
    }
}

function handleStatusUpdatedEvent(event) {
    const scenario = TrackedScenarios[event.id];
    if (scenario) {
        const statusText = getStatusDisplayName(event.status, event.outcome);
        scenario.setStatusText(statusText);
        scenario.setStatusColor(getStatusColor(statusText));
        scenario.setScenarioColor(getScenarioColor(statusText));
    }
}

function handleResultUpdatedEvent(event) {
    const scenario = TrackedScenarios[event.id];
    if (scenario) {
        scenario.results = event.result;
        const statusText = getStatusDisplayName(event.status, event.outcome);
        scenario.setStatusText(statusText);
        scenario.setStatusColor(getStatusColor(statusText));
        scenario.setScenarioColor(getScenarioColor(statusText));
    }
}

/**
 * Fetches all tracked scenarios from the API.
 * @returns {Promise<void>}
 */
async function fetchExistingScenarios() {
    const url = location.href + 'api/v1/scenarios';
    const response = await fetch(url);

    if (!response.ok) {
        console.error(`Error fetching scenarios: HTTP ${response.status} (${response.statusText})`);
        return;
    }

    const scenarios = await response.json();
    for (const scenario of scenarios) {
        handleScenarioCreatedEvent(scenario);
    }
}

function subscribeEvents() {
    if (typeof EventSource !== 'undefined') {
        const url = location.href + 'api/v1/events'
        const source = new EventSource(url);
        source.onmessage = function(event) {
            const data = JSON.parse(event.data);
            switch (data.event) {
                case 'scenario-created':
                    handleScenarioCreatedEvent(data);
                    break;
                case 'scenario-deleted':
                    handleScenarioDeletedEvent(data);
                    break;
                case 'status-updated':
                    handleStatusUpdatedEvent(data);
                    break;
                case 'result-updated':
                    handleResultUpdatedEvent(data);
                    break;
            }
        };
    } else {
        // TODO: Fallback to fetching continuously
        console.log('No support for server-sent events.');
    }
}

async function deleteAllScenarioResults() {
    const url = location.href + 'api/v1/result';
    const response = await fetch(url, {
        method: 'DELETE',
    });

    if (!response.ok) {
        console.error(`Error deleting results: HTTP ${response.status} (${response.statusText})`);
    }
}

async function deleteScenarioResults(scenarioId) {
    const url = location.href + `api/v1/result?id=${scenarioId}`;
    const response = await fetch(url, {
        method: 'DELETE',
    });

    if (!response.ok) {
        console.error(`Error deleting results: HTTP ${response.status} (${response.statusText})`);
    }
}

function bindModalListeners() {
    const modal = document.getElementById('scenario-result-modal');
    const downloadButton = document.getElementById('download-result__button');
    const deleteButton = document.getElementById('delete-result__button');
    if (modal && downloadButton && deleteButton) {
        modal.addEventListener('show.bs.modal', event => {
            // Element that triggered the modal
            const source = event.relatedTarget;
            // Extract info from data-bs-* attributes
            const scenarioId = source.getAttribute('data-bs-scenarioId');

            // Prepare modal data
            const scenario = TrackedScenarios[scenarioId];
            const hasResults = scenario?.results !== undefined && scenario?.results !== null;
            const resultText = hasResults
                ? JSON.stringify(scenario.results, null, 2)
                : 'N/A';

            // Update the modal's content
            if (hasResults) {
                downloadButton.classList.remove('disabled');
                downloadButton.href = `/api/v1/result?id=${scenarioId}`;
                deleteButton.onclick = () => deleteScenarioResults(scenarioId);
                deleteButton.classList.remove('disabled');
            } else {
                downloadButton.classList.add('disabled');
                downloadButton.href = `#`;
                deleteButton.onclick = () => {};
                deleteButton.classList.add('disabled');
            }

            const textarea = document.getElementById('scenario-result__textarea');
            if (textarea) {
                textarea.innerText = resultText;
            }
        })
    }
}

function bindThemeToggle() {
    document.getElementById('theme-toggle__button').addEventListener('click', () => {
        if (document.documentElement.getAttribute('data-bs-theme') !== 'dark') {
            applyColorTheme('dark', true);
        } else {
            applyColorTheme('light', true);
        }
    });
}

(async () => {
    loadColorTheme();
    bindThemeToggle();
    bindModalListeners();
    await fetchExistingScenarios();
    subscribeEvents();
})();
