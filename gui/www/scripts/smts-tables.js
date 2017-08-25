// Set of functions that manipulate the tables DOM objects
smts.tables = {

    // Set of functions that manipulate the DOM object 'smts-data-container'
    data: {

        // Make a button for requesting CNF of currently selected node
        // @return {HtmlNode}: The HTML button.
        makeGetClausesBtn: function() {
            let getClausesBtn = document.createElement('div');
            getClausesBtn.id = 'smts-data-get-clauses';
            getClausesBtn.className = 'btn btn-default btn-xs';
            getClausesBtn.innerHTML = 'Get Clauses';
            let instanceName = smts.tables.instances.getSelected();
            let nodePath = JSON.stringify(smts.tree.getSelectedNodes()[0].name);
            getClausesBtn.addEventListener('click', () => smts.cnf.load(instanceName, nodePath, null));
            return getClausesBtn;
        },

        // Make an event object with the wanted attributes for the data table
        // @param {TreeManager.Node} node: The mold event.
        // @return {Object}: The object representing the event, to be put in
        // the data table.
        makeItemEvent: function(event) {
            let itemEvent = {};
            if (event && event.data) {
                for (let key in event.data) {
                    if (key !== 'name' && key !== 'node') {
                        itemEvent[key] = event.data[key];
                    }
                }
            }
            return itemEvent;
        },

        // Make a node object with the wanted attributes for the data table
        // @param {TreeManager.Node} node: The mold node.
        // @return {Object}: The object representing the node, to be put in the
        // data table.
        makeItemNode: function(node) {
            let itemNode = {};
            if (node) {
                itemNode.status = node.status;
                itemNode.solvers = node.solvers.length;
                itemNode.balanceness = node.getBalanceness();
                if (node.info) {
                    for (let key in node.info) {
                        itemNode[key] = node.info[key];
                    }
                }
            }
            return itemNode;
        },

        // Make a solver object with the wanted attributes for the data table
        // @param {TreeManager.Solver} solver: The mold solver.
        // @return {Object}: The object representing the solver, to be put in
        // the data table.
        makeItemSolver: function(solver) {
            let itemSolver = {};
            if (solver) {
                // Do nothing
            }
            return itemSolver;
        },

        // Make table containing an object
        // It creates a DOM table element, with first colon being the key/index
        // of the given object/array, and the second the value. The function is
        // recursive, meaning if a value is an object/array, another table will
        // be created inside the cell.
        // @param {Object} item: The object to b converted into a HTML table.
        // @return {HtmlElement}: A DOM table element representing the object.
        makeTable: function(item) {
            // If item is an object or an array, create a table element
            if (item && typeof item === 'object') {
                let table = document.createElement('table');
                table.id = 'smts-data-table';
                table.classList.add('smts-table');

                // Make header, only if object/array has keys/elements
                if (Object.keys(item).length > 0) {
                    let header = table.createTHead().insertRow(0);
                    header.appendChild(document.createElement('th')).innerText = 'Key';
                    header.appendChild(document.createElement('th')).innerText = 'Value';
                }

                // Populate table
                let body = table.appendChild(document.createElement('tbody'));
                for (let key in item) {
                    let row = body.appendChild(document.createElement('tr'));
                    row.insertCell(0).innerText = key;
                    row.insertCell(1).appendChild(this.makeTable(item[key]));
                }

                return table;
            }

            // If item is a number, boolean or string, create a simple element
            let element = document.createElement('span');
            element.innerText = item;
            return element;
        },

        // Replace data table and change title of container
        // @param {TreeManager.*} item: A solver, event or node. The item will
        // be represented in the table.
        // @param {String} itemType: The type of the item. It can be one of:
        // 'solver', 'node', 'event +', 'event -', 'event status', 'event and',
        // 'event or' or 'event solved'.
        update: function(item, itemType) {
            document.getElementById('smts-data-title').innerHTML = itemType;
            let dataTableContainer = document.getElementById('smts-data-table-container');

            // Reset container
            dataTableContainer.innerHTML = '';

            // Create object to put in data table
            let dataTableItem = {};

            switch (itemType) {
                case 'event':
                    dataTableItem = this.makeItemEvent(item);
                    break;

                case 'node':
                    dataTableItem = this.makeItemNode(item);
                    dataTableContainer.appendChild(this.makeGetClausesBtn());
                    break;

                case 'solver':
                    dataTableItem = this.makeItemSolver(item);
                    break;
            }

            // Create and insert new table
            let dataTable = this.makeTable(dataTableItem);
            dataTableContainer.appendChild(dataTable);
        }

    },


    // Set of functions that manipulate the DOM object 'smts-events-container'
    events: {

        // Make cells of events table with selected nodes bold
        // @param {Node[]} selectedNodes: List of nodes for which corresponding
        // events' cells have to be selected.
        boldSelected: function(selectedNodes) {
            // Remove bold from all event cells
            let rows = this.getRows('>td.smts-bold');
            if (rows) rows.forEach(row => row.classList.remove('smts-bold'));

            // Apply bold to event cells with selected nodes
            for (let selectedNode of selectedNodes) {
                let selectedNodeNameStr = JSON.stringify(selectedNode.name);
                // node
                rows = this.getRows(`[data-node="${selectedNodeNameStr}"]`);
                if (rows) rows.forEach(row => row.children[2].classList.add('smts-bold'));
                // data-node
                rows = this.getRows(`[data-data-node="${selectedNodeNameStr}"]`);
                if (rows) rows.forEach(row => row.children[4].classList.add('smts-bold'));
            }
        },

        // Get rows of events table
        // @params {String} [optional] option: An extension to the original
        // query. If no option is provided, all rows are selected. If an option
        // is given, only rows matching the option are selected.
        // E.g.: option = '[data-node="[0,0]"]' selects only rows with
        // attribute `data-node` matching "[0.,0]".
        getRows: function(option = '') {
            let queryRows = '#smts-events-table > tbody > tr';
            return document.querySelectorAll(`${queryRows}${option}`)
        },

        // Highlight all rows with event matching one of events
        // @param {TreeManager.Events[]} events: List of events to be
        // highlighted.
        highlight: function(events) {
            // Remove highlight from all events
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.remove('smts-highlight'));

            // Highlight selected nodes
            for (let event of events) {
                rows = this.getRows(`[data-event="${event.id}"]`);
                if (rows) rows.forEach(row => row.classList.add('smts-highlight'));
            }
        },

        // Check if container scroll is at bottom
        isScrollBottom: function() {
            let container = document.getElementById('smts-events-table-container');
            return container.scrollTop === container.scrollHeight - container.offsetHeight;
        },

        // Check if a tab of the events container is selected
        // @param {String} tabName: the name of the tab. Has to be lowercase
        // for HTML id compatibility.
        // @return {Boolean}: `true` if the tab is active, `false` otherwise.
        isTabActive: function(tabName) {
            let tab = document.getElementById(`smts-events-navbar-${tabName}`);
            return tab ? tab.classList.contains('active') : false;
        },

        scroll: function(event) {
            let rows = this.getRows(`[data-event="${event.id}"]`);
            if (rows && rows[0]) {
                let container = document.getElementById('smts-events-table-container');
                container.scrollTop = rows[0].offsetTop;
            }
        },

        // Scroll events table container to bottom
        scrollBottom: function() {
            let container = document.getElementById('smts-events-table-container');
            container.scrollTop = container.scrollHeight - container.offsetHeight;
        },

        // Highlight next element in events table
        // Allows events navigation through up and down arrow keys while focus
        // is on events table.
        // @param {string} direction: Can be 'up' or 'down'.
        shift: function(direction) {
            let selectedEvents = this.getRows(`.smts-highlight`);

            if (selectedEvents && selectedEvents[0]) {
                let selected = selectedEvents[0];
                let sibling = direction === 'up' ?
                    selected.previousElementSibling : selected.nextElementSibling;

                if (sibling) {
                    selected.classList.remove('smts-highlight');
                    sibling.classList.add('smts-highlight');

                    // Update tree only if some time has passed, to avoid
                    // generating the tree many times while holding arrows
                    // to move faster in the list.
                    if (this.shiftTimeout) window.clearTimeout(this.shiftTimeout);
                    this.shiftTimeout = window.setTimeout(() => sibling.click(), 1500);

                    // Scroll events table to right position if next selected
                    // is out of visible frame.
                    let container = document.getElementById('smts-events-table-container');
                    if (sibling.offsetTop < container.scrollTop) {
                        container.scrollTop = sibling.offsetTop;
                    } else if (container.scrollTop + container.offsetHeight <
                        sibling.offsetTop + sibling.offsetHeight) {
                        container.scrollTop =
                            sibling.offsetTop + sibling.offsetHeight - container.offsetHeight;
                    }
                }
            }
        },

        // Show all rows in events table
        showAll: function() {
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.remove('smts-hidden'));
        },

        // Show rows of events table for which the event node matches at least
        // one of the selected nodes
        // @param {Node[]} selectedNodes: List of nodes for which corresponding
        // events have to be selected.
        showSelected: function(selectedNodes) {
            // Hide all nodes
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.add('smts-hidden'));

            // Show nodes that are in event.node or event.data.node
            for (let selectedNode of selectedNodes) {
                let selectedNodeNameStr = JSON.stringify(selectedNode.name);
                // node
                rows = this.getRows(`[data-node="${selectedNodeNameStr}"]`);
                if (rows) rows.forEach(row => row.classList.remove('smts-hidden'));
                // data-node
                rows = this.getRows(`[data-data-node="${selectedNodeNameStr}"]`);
                if (rows) rows.forEach(row => row.classList.remove('smts-hidden'));
            }
        },

        // Show all or selected nodes of events table, depending on selected tab
        // @param {Node[]} selectedNodes: List of nodes to compare to each
        // row's node.
        update: function(selectedNodes) {
            this.boldSelected(selectedNodes);
            this.isTabActive('selected') ? this.showSelected(selectedNodes) : this.showAll();
        }
    },


    // Set of functions that manipulate the DOM object 'smts-instances-container'
    instances: {

        // Make cells of instances table matching one of given instances bold
        // @param {Node[]} [optional] instances: List of instances for which
        // corresponding instances' cells have to be selected. The default
        // value is an empty array, if no instances have to be selected.
        bold: function(instances = []) {
            // Remove bold from all event cells
            let rows = this.getRows('>td.smts-bold');
            if (rows) rows.forEach(row => row.classList.remove('smts-bold'));

            // Apply bold to instances cells with executing instance
            for (let instance of instances) {
                rows = this.getRows(`[data-instance="${instance.name}"]`);
                if (rows) rows.forEach(row => row.children[0].classList.add('smts-bold'));
            }
        },

        // Get selected instance name
        // @return {string}: The name of the current instance, the empty string
        // if no instance is selected.
        getSelected: function() {
            let rows = this.getRows('.smts-highlight');
            if (rows && rows[0]) {
                return rows[0].getAttribute('data-instance');
            }
            return '';
        },

        // Get rows of instances table
        // @params {String} [optional] option: An extension to the original
        // query. If no option is provided, all rows are selected. If an option
        // is given, only rows matching the option are selected.
        // E.g.: option = '[data-instance="ABCD"]' selects only rows with
        // attribute `data-instance` matching "ABCD".
        getRows: function(option = '') {
            let queryRows = '#smts-instances-table > tbody > tr';
            return document.querySelectorAll(`${queryRows}${option}`)
        },

        // Highlight all rows with instance matching one of instances
        // @param {Instance[]} instances: List of instance to be highlighted.
        highlight: function(instances) {
            // Remove highlight from all instances
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.remove('smts-highlight'));

            for (let instance of instances) {
                rows = this.getRows(`[data-instance="${instance.name}"]`);
                if (rows) rows.forEach(row => row.classList.add('smts-highlight'));
            }
        }
    },


    // Set of functions that manipulate the DOM object 'smts-solvers-container'
    solvers: {

        // Make cells of solvers table with selected nodes bold
        // @param {Node[]} selectedNodes: List of nodes for which corresponding
        // solvers' cells have to be selected.
        boldSelected: function(selectedNodes) {
            // Remove bold from all event cells
            let rows = this.getRows('>td.smts-bold');
            if (rows) rows.forEach(row => row.classList.remove('smts-bold'));

            // Apply bold to solver cells with selected nodes
            for (let selectedNode of selectedNodes) {
                let selectedNodeNameStr = JSON.stringify(selectedNode.name);
                rows = this.getRows(`[data-node="${selectedNodeNameStr}"]`);
                if (rows) rows.forEach(row => row.children[1].classList.add('smts-bold'));
            }
        },

        // Get rows of solvers table
        // @params {String} [optional] option: An extension to the original
        // query. If no option is provided, all rows are selected. If an option
        // is given, only rows matching the option are selected.
        // E.g.: option = '[data-node="[0,0]"]' selects only rows with
        // attribute `data-node` matching "[0.,0]".
        getRows: function(option = '') {
            let queryRows = '#smts-solvers-table > tbody > tr';
            return document.querySelectorAll(`${queryRows}${option}`)
        },

        // Get name of selected solver
        // @return {string}: Name of the selected solver, the empty string if
        // no solver is selected.
        getSelected: function() {
            let rows = this.getRows('.smts-highlight'); // Get selected
            if (rows && rows[0]) {
                console.log(rows[0].getAttribute('data-solver'));
                return rows[0].getAttribute('data-solver');
            }
            console.log('NOTHING');
            return '';
        },

        // Highlight all rows with solver matching one of solvers
        // @param {TreeManager.Solver[]} solvers: List of solvers to be
        // highlighted.
        highlight: function(solvers) {
            // Remove highlight from all solvers
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.remove('smts-highlight'));

            // Highlight selected nodes
            for (let solver of solvers) {
                rows = this.getRows(`[data-solver='${solver.name}']`);
                if (rows) rows.forEach(row => row.classList.add('smts-highlight'));
            }
        },

        // Check if a tab of the solvers container is selected
        // @param {String} tabName: The name of the tab. Has to be lowercase
        // for HTML id compatibility.
        // @return {Boolean}: `true` if the tab is active, `false` otherwise.
        isTabActive: function(tabName) {
            let tab = document.getElementById(`smts-solvers-navbar-${tabName}`);
            return tab ? tab.classList.contains('active') : false;
        },

        // Show all rows in solvers table
        showAll: function() {
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.remove('smts-hidden'));
        },

        // Show rows of solvers table for which the solver node matches at
        // least one of the selected nodes
        // @param {Node[]} selectedNodes: List of nodes to compare to each
        // row's node.
        showSelected: function(selectedNodes) {
            // Hide all nodes
            let rows = this.getRows();
            if (rows) rows.forEach(row => row.classList.add('smts-hidden'));

            // Show nodes that are in solver.node
            for (let selectedNode of selectedNodes) {
                let selectedNodeNameStr = JSON.stringify(selectedNode.name);
                rows = this.getRows(`[data-node="${selectedNodeNameStr}"]`);
                if (rows) rows.forEach(row => row.classList.remove('smts-hidden'));
            }
        },

        // Show all or selected nodes of solvers table, depending on selected tab
        // @param {Node[]} selectedNodes: List of nodes to compare to each
        // row's node.
        update: function(selectedNodes) {
            this.boldSelected(selectedNodes);
            this.isTabActive('selected') ? this.showSelected(selectedNodes) : this.showAll();
        }
    }
};