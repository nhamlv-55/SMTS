smts.timeline = {

    events: [],

    clear: function() {
        let line = document.getElementById('smts-timeline-line');
        if (line) line.innerHTML = '';
    },

    getEventIdBytTime: function(time) {
        for (let i = 0; i < this.events.length; i++) {
            if (this.events[i].ts === time) {
                return this.events[i].id;
            }
        }
        return null;
    },

    make: function(events) {
        this.trimEvents(events);

        let line = document.getElementById('smts-timeline-line');
        if (!line) return;

        // Hide timeline if there's less than two events
        if (this.events.length < 2) {
            line.classList.add('smts-hidden');
        }
        else {
            // Show timeline in case it was hidden before
            line.classList.remove('smts-hidden');

            let lineLength = 1 / this.events[this.events.length - 1].ts;

            for (let event of this.events) {
                // Make dash
                let dash = document.createElement('div');
                dash.id = `smts-timeline-event-${event.id}`;
                dash.classList.add('smts-timeline-dash');
                dash.setAttribute('data-event', event.id);
                dash.style.left = `${lineLength * event.ts * 100}%`;

                // Set events
                dash.addEventListener('mouseenter', function() {
                    dash.classList.add('hover');
                });

                dash.addEventListener('mouseleave', function() {
                    dash.classList.remove('hover');
                });

                dash.addEventListener('click', function() {
                    smts.timeline.selectEvent(event.id);

                    let eventRow = smts.tables.events.getRows(`[data-event="${event.id}"]`)[0];
                    if (eventRow) {
                        // Simulate event click to rebuild the tree
                        eventRow.click();
                        // TODO: add scroll
                    }
                });

                // Make popup span
                let popupSpan = document.createElement('div');
                popupSpan.classList.add('smts-timeline-popupSpan');
                popupSpan.innerText = `${event.ts} s`;

                // Insert elements
                dash.appendChild(popupSpan);
                line.appendChild(dash);
            }
        }

        // Select first dash
        let firstDash = line.children[0];
        if (firstDash) {
            firstDash.classList.add('smts-timeline-active');
        }
    },

    selectEvent: function(eventId) {
        // Unselect previously selected element
        let activeEvents = document.querySelectorAll('.smts-timeline-active');
        for (let activeEvent of activeEvents) {
            activeEvent.classList.remove('smts-timeline-active');
        }

        // Get element and select it
        let event = document.getElementById(`smts-timeline-event-${eventId}`);
        if (event) {
            event.classList.add('smts-timeline-active');
        }
    },

    trimEvents: function(events) {
        this.events = [];
        this.events.push(events[0]);
        for (let i = 1; i < events.length - 1; i++) {
            // Push event only if time is different from last inserted
            if (this.events[this.events.length - 1].ts !== events[i].ts) {
                this.events.push(events[i]);
            }
        }
    },

    update: function(event) {
        let dash = document.getElementById(`smts-timeline-event-${event.id}`);
        if (dash !== null) {
            this.selectEvent(event.id);
        }
        else if (this.events.length > 1) {
            this.selectEvent(this.getEventIdBytTime(event.ts));
        }
    }

};