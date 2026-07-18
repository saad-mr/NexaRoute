const SVG_NS = "http://www.w3.org/2000/svg";

const ACTIVE_DELIVERY_STATUSES = ["Rider Assigned", "Rider Arriving", "Picked Up", "In Transit"];
const FINAL_DELIVERY_STATUSES = ["Delivered", "Cancelled"];
const PROFILE_COPY = {
    customer: ["CU", "Customer portal", "Book and track parcels"],
    rider: ["RD", "Rider workspace", "Manage jobs and pricing"],
    admin: ["AD", "Admin portal", "Network and business controls"]
};

function readStoredJson(key, fallback) {
    try {
        const stored = window.localStorage.getItem(key);
        return stored ? JSON.parse(stored) : fallback;
    } catch (_) {
        return fallback;
    }
}

function readSessionText(key) {
    try {
        return window.sessionStorage.getItem(key) || "";
    } catch (_) {
        return "";
    }
}

function storeSessionText(key, value) {
    try {
        if (value) window.sessionStorage.setItem(key, value);
        else window.sessionStorage.removeItem(key);
    } catch (_) {
        // Sessions still work until the current page is refreshed.
    }
}

const state = {
    city: null,
    riders: [],
    bookings: [],
    dashboard: null,
    adminRiders: [],
    adminBookings: [],
    adminDashboard: null,
    adminAnalytics: null,
    routeComparison: null,
    activeRouteMode: "minimumDistance",
    latestBooking: null,
    trackedBooking: null,
    activeRole: "customer",
    riderSessionToken: readSessionText("nexaroute-rider-session"),
    adminSessionToken: readSessionText("nexaroute-admin-session"),
    authenticatedRider: null,
    notifications: readStoredJson("nexaroute-notifications", []),
    knownBookingStatuses: new Map(),
    bookingSyncReady: false,
    toastTimer: null,
    pollBusy: false,
    pollFailures: 0,
    pollTimer: null,
    deliveryAnimationFrame: null,
    renderedDeliveryKey: "",
    approachRoutes: new Map()
};

const byId = id => document.getElementById(id);
const elements = {
    serverPulse: byId("server-pulse"),
    serverLabel: byId("server-label"),
    currentDate: byId("current-date"),
    cityMap: byId("city-map"),
    mapLoading: byId("map-loading"),
    roadLayer: byId("road-layer"),
    routeLayer: byId("route-layer"),
    locationLayer: byId("location-layer"),
    riderLayer: byId("rider-layer"),
    deliveryLayer: byId("delivery-layer"),
    deliveryMapStatus: byId("delivery-map-status"),
    deliveryMapTracking: byId("delivery-map-tracking"),
    deliveryMapPhase: byId("delivery-map-phase"),
    deliveryMapDetail: byId("delivery-map-detail"),
    deliveryMapProgress: byId("delivery-map-progress"),
    roadCount: byId("road-count"),
    roadSummary: byId("road-summary"),
    locationCount: byId("location-count"),
    availableRiders: byId("available-riders"),
    riderSummary: byId("rider-summary"),
    totalBookings: byId("total-bookings"),
    bookingSummary: byId("booking-summary"),
    activeDeliveries: byId("active-deliveries"),
    deliverySummary: byId("delivery-summary"),
    pickup: byId("pickup-select"),
    destination: byId("destination-select"),
    routeForm: byId("route-form"),
    routeResults: byId("route-results"),
    foundationNote: byId("foundation-note"),
    distancePath: byId("distance-path"),
    distanceKm: byId("distance-km"),
    distanceStops: byId("distance-stops"),
    stopsPath: byId("stops-path"),
    stopsCount: byId("stops-count"),
    stopsKm: byId("stops-km"),
    bookingModal: byId("booking-modal"),
    bookingDetailsView: byId("booking-details-view"),
    offerSelectionView: byId("offer-selection-view"),
    confirmationView: byId("booking-confirmation-view"),
    bookingDetailsForm: byId("booking-details-form"),
    offerList: byId("offer-list"),
    trackingModal: byId("tracking-modal"),
    trackingSearchForm: byId("tracking-search-form"),
    trackingInput: byId("tracking-id-input"),
    trackingEmpty: byId("tracking-empty"),
    trackingResult: byId("tracking-result"),
    deliveryRatingCard: byId("delivery-rating-card"),
    deliveryRatingForm: byId("delivery-rating-form"),
    deliveryRatingSelect: byId("delivery-rating-select"),
    deliveryRatingComplete: byId("delivery-rating-complete"),
    automaticDeliveryNote: byId("automatic-delivery-note"),
    automaticStatusCountdown: byId("automatic-status-countdown"),
    roadModal: byId("road-modal"),
    roadSelect: byId("road-select"),
    trafficSelect: byId("traffic-select"),
    operationsModal: byId("operations-modal"),
    operationsContent: byId("operations-content"),
    notificationModal: byId("notification-modal"),
    notificationList: byId("notification-list"),
    accessModal: byId("access-modal"),
    riderAuthChoice: byId("rider-auth-choice"),
    riderLoginForm: byId("rider-login-form"),
    adminLoginForm: byId("admin-login-form"),
    toast: byId("toast"),
    toastTitle: byId("toast-title"),
    toastMessage: byId("toast-message"),
    riderProfileForm: byId("rider-profile-form"),
    riderRegistrationForm: byId("rider-registration-form"),
    riderActiveJob: byId("rider-active-job"),
    adminAddRoadForm: byId("admin-add-road-form"),
    adminAddLocationForm: byId("admin-add-location-form"),
    adminRecentBookings: byId("admin-recent-bookings")
};

function svgElement(tag, attributes = {}) {
    const element = document.createElementNS(SVG_NS, tag);
    Object.entries(attributes).forEach(([name, value]) => element.setAttribute(name, value));
    return element;
}

function textElement(tag, className, value) {
    const element = document.createElement(tag);
    if (className) element.className = className;
    element.textContent = value;
    return element;
}

async function getJson(path) {
    const response = await fetch(path, { cache: "no-store" });
    if (!response.ok) {
        let message = `Request failed with status ${response.status}`;
        try {
            const payload = await response.json();
            message = payload.error || message;
        } catch (_) {
            // Keep the HTTP status message.
        }
        throw new Error(message);
    }
    return response.json();
}

async function postForm(path, values) {
    const response = await fetch(path, {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
        body: new URLSearchParams(values)
    });
    const payload = await response.json();
    if (!response.ok) throw new Error(payload.error || `Request failed with status ${response.status}`);
    return payload;
}

function showAuthView(view) {
    elements.riderAuthChoice.hidden = view !== "rider-choice";
    elements.riderLoginForm.hidden = view !== "rider-login";
    elements.riderRegistrationForm.hidden = view !== "rider-register";
    elements.adminLoginForm.hidden = view !== "admin-login";
    byId("access-modal-eyebrow").textContent = view === "admin-login" ? "Protected operations" : "Secure rider workspace";
    byId("access-modal-title").textContent = view === "admin-login" ? "Admin login" : view === "rider-register" ? "Register as a rider" : view === "rider-login" ? "Rider login" : "Rider access";
}

function openAccess(role) {
    showAuthView(role === "admin" ? "admin-login" : "rider-choice");
    openModal(elements.accessModal);
}

async function loadAdminData() {
    if (!state.adminSessionToken) throw new Error("Admin login is required.");
    const token = encodeURIComponent(state.adminSessionToken);
    const [dashboard, riderPayload, bookingPayload, analytics] = await Promise.all([
        getJson(`/api/admin/dashboard?token=${token}`),
        getJson(`/api/admin/riders?token=${token}`),
        getJson(`/api/admin/bookings?token=${token}`),
        getJson(`/api/admin/analytics?token=${token}`)
    ]);
    state.adminDashboard = dashboard;
    state.adminRiders = riderPayload.riders || [];
    state.adminBookings = bookingPayload.bookings || [];
    state.adminAnalytics = analytics;
}

async function requestWorkspace(role) {
    if (role === "customer") {
        setActiveRole("customer");
        return;
    }
    if (role === "rider") {
        if (!state.riderSessionToken || !state.authenticatedRider) {
            openAccess("rider");
            return;
        }
        setActiveRole("rider");
        return;
    }
    if (!state.adminSessionToken) {
        openAccess("admin");
        return;
    }
    try {
        await loadAdminData();
        setActiveRole("admin");
    } catch (error) {
        state.adminSessionToken = "";
        storeSessionText("nexaroute-admin-session", "");
        openAccess("admin");
        showToast("Admin session expired", error.message, true);
    }
}

async function loginRider(event) {
    event.preventDefault();
    const submit = elements.riderLoginForm.querySelector("button[type=submit]");
    submit.disabled = true;
    try {
        const payload = await postForm("/api/auth/rider/login", {
            riderId: byId("rider-login-id").value.trim(),
            password: byId("rider-login-password").value
        });
        state.riderSessionToken = payload.session.token;
        state.authenticatedRider = payload.rider;
        storeSessionText("nexaroute-rider-session", state.riderSessionToken);
        elements.riderLoginForm.reset();
        closeModal(elements.accessModal);
        setActiveRole("rider");
        showToast("Rider login successful", `Welcome back, ${payload.rider.name}.`);
    } catch (error) {
        showToast("Rider login failed", error.message, true);
    } finally {
        submit.disabled = false;
    }
}

async function loginAdmin(event) {
    event.preventDefault();
    const submit = elements.adminLoginForm.querySelector("button[type=submit]");
    submit.disabled = true;
    try {
        const payload = await postForm("/api/auth/admin/login", {
            username: byId("admin-login-username").value.trim(),
            password: byId("admin-login-password").value
        });
        state.adminSessionToken = payload.session.token;
        storeSessionText("nexaroute-admin-session", state.adminSessionToken);
        byId("admin-login-password").value = "";
        await loadAdminData();
        closeModal(elements.accessModal);
        setActiveRole("admin");
        showToast("Admin access granted", "The protected operations workspace is ready.");
    } catch (error) {
        state.adminSessionToken = "";
        storeSessionText("nexaroute-admin-session", "");
        showToast("Admin login failed", error.message, true);
    } finally {
        submit.disabled = false;
    }
}

async function logout(role) {
    const token = role === "admin" ? state.adminSessionToken : state.riderSessionToken;
    if (token) {
        try { await postForm("/api/auth/logout", { token }); } catch (_) { /* Clear local access either way. */ }
    }
    if (role === "admin") {
        state.adminSessionToken = "";
        state.adminDashboard = null;
        state.adminRiders = [];
        state.adminBookings = [];
        state.adminAnalytics = null;
        storeSessionText("nexaroute-admin-session", "");
    } else {
        state.riderSessionToken = "";
        state.authenticatedRider = null;
        storeSessionText("nexaroute-rider-session", "");
    }
    setActiveRole("customer");
    showToast("Signed out", `${role === "admin" ? "Admin" : "Rider"} session closed safely.`);
}

async function restoreSessions() {
    if (state.riderSessionToken) {
        try {
            const payload = await getJson(`/api/auth/session?token=${encodeURIComponent(state.riderSessionToken)}`);
            if (payload.session.role === "Rider" && payload.rider) state.authenticatedRider = payload.rider;
            else throw new Error("Invalid rider session");
        } catch (_) {
            state.riderSessionToken = "";
            state.authenticatedRider = null;
            storeSessionText("nexaroute-rider-session", "");
        }
    }
    if (state.adminSessionToken) {
        try {
            const payload = await getJson(`/api/auth/session?token=${encodeURIComponent(state.adminSessionToken)}`);
            if (payload.session.role !== "Admin") throw new Error("Invalid admin session");
        } catch (_) {
            state.adminSessionToken = "";
            storeSessionText("nexaroute-admin-session", "");
        }
    }
}

function setConnection(connected, label) {
    elements.serverPulse.classList.toggle("connected", connected);
    elements.serverPulse.classList.toggle("failed", !connected);
    elements.serverLabel.textContent = label;
}

function showToast(title, message, error = false) {
    window.clearTimeout(state.toastTimer);
    elements.toastTitle.textContent = title;
    elements.toastMessage.textContent = message;
    elements.toast.classList.toggle("error", error);
    elements.toast.classList.add("visible");
    state.toastTimer = window.setTimeout(() => elements.toast.classList.remove("visible"), 4200);
}

function initials(name) {
    return String(name || "Rider")
        .split(/\s+/)
        .filter(Boolean)
        .slice(0, 2)
        .map(part => part[0].toUpperCase())
        .join("");
}

function money(value) {
    return `Rs. ${Number(value || 0).toLocaleString("en-PK", { maximumFractionDigits: 0 })}`;
}

function ratingLabel(value) {
    const rating = Number(value);
    return Number.isFinite(rating) && rating > 0
        ? `★ ${rating.toFixed(1)}`
        : "Not rated";
}

function openModal(modal) {
    modal.hidden = false;
    document.body.style.overflow = "hidden";
}

function closeModal(modal) {
    if (!modal) return;
    modal.hidden = true;
    if ([...document.querySelectorAll(".modal")].every(item => item.hidden)) {
        document.body.style.overflow = "";
    }
}

function saveNotifications() {
    try {
        window.localStorage.setItem("nexaroute-notifications", JSON.stringify(state.notifications));
    } catch (_) {
        // Notifications remain available for the current session.
    }
}

function notificationSymbol(type) {
    return ({ delivery: "↗", booking: "P", rider: "R", road: "N", success: "✓" })[type] || "•";
}

function formatNotificationTime(timestamp) {
    return new Intl.DateTimeFormat("en-PK", { hour: "2-digit", minute: "2-digit" }).format(new Date(timestamp));
}

function renderNotifications() {
    const unread = state.notifications.filter(item => item.unread).length;
    document.querySelectorAll("[data-notification-count]").forEach(counter => {
        counter.textContent = unread > 99 ? "99+" : String(unread);
        counter.hidden = unread === 0;
    });

    elements.notificationList.replaceChildren();
    if (state.notifications.length === 0) {
        const empty = document.createElement("div");
        empty.className = "notification-empty";
        const icon = document.createElement("span");
        icon.textContent = "○";
        empty.append(icon, textElement("strong", "", "No notifications yet"), textElement("p", "", "Booking and delivery updates will appear here."));
        elements.notificationList.append(empty);
        return;
    }

    state.notifications.forEach(item => {
        const row = document.createElement(item.trackingId ? "button" : "article");
        if (item.trackingId) row.type = "button";
        row.className = `notification-item${item.unread ? " unread" : ""}`;
        const icon = textElement("span", "notification-item-icon", notificationSymbol(item.type));
        const copy = document.createElement("div");
        copy.append(textElement("strong", "", item.title), textElement("p", "", item.message));
        row.append(icon, copy, textElement("time", "", formatNotificationTime(item.time)));
        if (item.trackingId) {
            row.addEventListener("click", () => {
                item.unread = false;
                saveNotifications();
                renderNotifications();
                closeModal(elements.notificationModal);
                setActiveRole("customer");
                openTrackingDialog(item.trackingId);
            });
        }
        elements.notificationList.append(row);
    });
}

function addNotification(title, message, type = "delivery", trackingId = "", uniqueKey = "") {
    if (uniqueKey && state.notifications.some(item => item.uniqueKey === uniqueKey)) return;
    state.notifications.unshift({
        id: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
        title,
        message,
        type,
        trackingId,
        uniqueKey,
        unread: true,
        time: Date.now()
    });
    state.notifications = state.notifications.slice(0, 40);
    saveNotifications();
    renderNotifications();
}

function openNotifications() {
    renderNotifications();
    openModal(elements.notificationModal);
}

function markNotificationsRead() {
    state.notifications.forEach(item => { item.unread = false; });
    saveNotifications();
    renderNotifications();
}

function setActiveRole(role) {
    if (!PROFILE_COPY[role]) role = "customer";
    state.activeRole = role;

    document.querySelectorAll(".role-panel").forEach(panel => {
        panel.hidden = panel.id !== `${role}-panel`;
    });
    document.querySelectorAll("[data-role-target]").forEach(button => {
        button.classList.toggle("active", button.dataset.roleTarget === role);
    });
    document.querySelectorAll("[data-role-nav]").forEach(item => {
        item.hidden = item.dataset.roleNav !== role;
    });
    document.querySelectorAll(".nav-item").forEach(item => item.classList.remove("active"));
    document.querySelector(`.nav-item[data-role-nav="${role}"]`)?.classList.add("active");

    let [avatar, name, detail] = PROFILE_COPY[role];
    if (role === "rider" && state.authenticatedRider) {
        avatar = initials(state.authenticatedRider.name);
        name = state.authenticatedRider.name;
        detail = `${state.authenticatedRider.id} · Rider profile`;
    }
    byId("profile-avatar").textContent = avatar;
    byId("profile-name").textContent = name;
    byId("profile-role").textContent = detail;
    if (role === "rider") renderRiderPanel();
    if (role === "admin") renderAdminPanel();
    window.scrollTo({ top: 0, behavior: "smooth" });
}

function locationOptions(locations, placeholder = "Select a location") {
    const sorted = [...locations].sort((a, b) => a.name.localeCompare(b.name));
    return [`<option value="">${placeholder}</option>`]
        .concat(sorted.map(location => `<option value="${location.id}">${location.name}</option>`))
        .join("");
}

function populateLocationSelects(locations, preserve = true) {
    const pickupValue = preserve ? elements.pickup.value : "";
    const destinationValue = preserve ? elements.destination.value : "";
    const locationList = byId("location-options");
    locationList.replaceChildren(...[...locations]
        .sort((a, b) => a.name.localeCompare(b.name))
        .map(location => {
            const option = document.createElement("option");
            option.value = location.name;
            option.label = location.type;
            return option;
        }));
    elements.pickup.value = locations.some(location => location.name.toLowerCase() === pickupValue.toLowerCase()) ? pickupValue : "";
    elements.destination.value = locations.some(location => location.name.toLowerCase() === destinationValue.toLowerCase()) ? destinationValue : "";

    const riderLocation = byId("new-rider-location");
    const riderLocationValue = riderLocation.value;
    riderLocation.innerHTML = locationOptions(locations, "Choose starting location");
    if (locations.some(location => String(location.id) === riderLocationValue)) riderLocation.value = riderLocationValue;

    const profileLocation = byId("rider-current-location");
    const profileLocationValue = state.authenticatedRider
        ? String(state.authenticatedRider.locationId)
        : profileLocation.value;
    profileLocation.innerHTML = locationOptions(locations, "Choose current location");
    if (locations.some(location => String(location.id) === profileLocationValue)) {
        profileLocation.value = profileLocationValue;
    }

    [byId("admin-road-from"), byId("admin-road-to"), byId("admin-location-anchor")].forEach(select => {
        const value = select.value;
        select.innerHTML = locationOptions(locations, "Choose location");
        if (locations.some(location => String(location.id) === value)) select.value = value;
    });
}

function roadClass(road) {
    if (road.blocked) return "blocked";
    return String(road.traffic || "Low").toLowerCase();
}

function renderCityMap(city, riders) {
    elements.roadLayer.replaceChildren();
    elements.routeLayer.replaceChildren();
    elements.locationLayer.replaceChildren();
    elements.riderLayer.replaceChildren();
    elements.deliveryLayer.replaceChildren();
    state.renderedDeliveryKey = "";

    const locationsById = new Map(city.locations.map(location => [location.id, location]));
    city.roads.forEach(road => {
        const start = locationsById.get(road.from);
        const end = locationsById.get(road.to);
        if (!start || !end) return;
        const base = svgElement("line", { x1: start.x, y1: start.y, x2: end.x, y2: end.y, class: "road-base" });
        const flow = svgElement("line", {
            x1: start.x, y1: start.y, x2: end.x, y2: end.y,
            class: `road-flow ${roadClass(road)}`, "data-road-id": road.id
        });
        const label = svgElement("text", {
            x: (start.x + end.x) / 2, y: (start.y + end.y) / 2 - 7,
            class: "road-label", "text-anchor": "middle"
        });
        label.textContent = `${road.distance} km`;
        elements.roadLayer.append(base, flow, label);
    });

    city.locations.forEach(location => {
        const group = svgElement("g", { class: "location-group", "data-location-id": location.id, tabindex: "0" });
        const hit = svgElement("circle", { cx: location.x, cy: location.y, r: 23, class: "location-hit" });
        const dot = svgElement("circle", {
            cx: location.x, cy: location.y, r: location.id === 0 ? 13 : 9,
            class: `location-dot${location.id === 0 ? " hub" : ""}`
        });
        const labelY = location.y + (location.y < 140 ? 30 : -21);
        const label = svgElement("text", { x: location.x, y: labelY, class: "location-label", "text-anchor": "middle" });
        label.textContent = location.name;
        const sub = svgElement("text", { x: location.x, y: labelY + 14, class: "location-sub", "text-anchor": "middle" });
        sub.textContent = location.type;
        group.append(hit, dot, label, sub);
        group.addEventListener("click", () => showToast(location.name, `${location.type} location in the Nexa City network.`));
        elements.locationLayer.append(group);
    });

    riders.filter(rider => rider.available).slice(0, 6).forEach((rider, index) => {
        const location = locationsById.get(rider.locationId);
        if (!location) return;
        const group = svgElement("g", {
            class: "rider-marker",
            transform: `translate(${location.x + 24 + (index % 2) * 7} ${location.y - 22})`,
            "data-rider-id": rider.id
        });
        group.append(
            svgElement("circle", { cx: 0, cy: 0, r: 20 }),
            svgElement("circle", { cx: 0, cy: 0, r: 13 }),
            svgElement("path", { d: "M-7 2h5l3-6m-5 6-3 5m8-11 5 6h-5m-8 5a3 3 0 1 0 0 .1m14-.1a3 3 0 1 0 0 .1" })
        );
        const title = svgElement("title");
        title.textContent = `${rider.name}, ${rider.vehicle}, ${ratingLabel(rider.rating).toLowerCase()}`;
        group.append(title);
        elements.riderLayer.append(group);
    });

    elements.mapLoading.classList.add("hidden");
    elements.roadCount.textContent = city.roads.length;
    elements.roadSummary.textContent = `${city.roads.filter(road => road.blocked).length} currently blocked`;
    elements.locationCount.textContent = city.locations.length;
    if (state.routeComparison) drawRoute(state.routeComparison[state.activeRouteMode], state.activeRouteMode);
    renderCurrentDelivery(true);
}

function compactPath(route) {
    if (!route?.path?.length) return "No available route";
    if (route.path.length <= 3) return route.path.map(item => item.name).join(" → ");
    return `${route.path[0].name} → ${route.path.length - 2} places → ${route.path.at(-1).name}`;
}

function drawRoute(route, mode) {
    elements.routeLayer.replaceChildren();
    if (!state.city || !route?.found || route.path.length < 2) return;
    const locations = new Map(state.city.locations.map(location => [location.id, location]));
    const points = route.path.map(item => locations.get(item.id)).filter(Boolean).map(location => `${location.x},${location.y}`).join(" ");
    elements.routeLayer.append(
        svgElement("polyline", { points, class: "route-halo" }),
        svgElement("polyline", { points, class: `route-highlight ${mode}`, pathLength: "1" })
    );
    route.path.forEach((item, index) => {
        const location = locations.get(item.id);
        if (!location) return;
        elements.routeLayer.append(svgElement("circle", {
            cx: location.x, cy: location.y,
            r: index === 0 || index === route.path.length - 1 ? 10 : 7,
            class: `route-waypoint ${mode}`,
            style: `animation-delay:${index * 80}ms`
        }));
    });
}

function showRouteComparison(comparison) {
    state.routeComparison = comparison;
    state.activeRouteMode = "minimumDistance";
    const distance = comparison.minimumDistance;
    const stops = comparison.minimumStops;
    elements.distancePath.textContent = compactPath(distance);
    elements.distanceKm.textContent = `${distance.distance.toFixed(1)} km`;
    elements.distanceStops.textContent = `${distance.stops} road${distance.stops === 1 ? "" : "s"} · ${distance.estimatedMinutes.toFixed(1)} min`;
    elements.stopsPath.textContent = compactPath(stops);
    elements.stopsCount.textContent = `${stops.stops} stop${stops.stops === 1 ? "" : "s"}`;
    elements.stopsKm.textContent = `${stops.distance.toFixed(1)} km · ${stops.estimatedMinutes.toFixed(1)} min`;
    elements.foundationNote.hidden = true;
    elements.routeResults.hidden = false;
    document.querySelectorAll(".route-option").forEach(option => {
        option.classList.toggle("selected", option.dataset.routeMode === "minimumDistance");
    });
    drawRoute(distance, "minimumDistance");
}

function progressForBooking(booking) {
    if (!booking.nextStatusAt || booking.nextStatusAt <= booking.statusChangedAt) {
        return booking.status === "Delivered" ? 1 : 0;
    }
    return Math.max(0, Math.min(1, (Date.now() / 1000 - booking.statusChangedAt) / (booking.nextStatusAt - booking.statusChangedAt)));
}

function locationPoint(locationId) {
    return state.city?.locations.find(location => location.id === locationId) || null;
}

function locationFromInput(value) {
    const normalized = String(value || "").trim().toLowerCase();
    return state.city?.locations.find(location => location.name.toLowerCase() === normalized) || null;
}

function routePoints(route) {
    if (!route?.path?.length) return [];
    return route.path.map(item => locationPoint(item.id)).filter(Boolean);
}

function interpolatePath(points, progress) {
    if (points.length === 0) return { x: 0, y: 0, angle: 0 };
    if (points.length === 1) return { x: points[0].x, y: points[0].y, angle: 0 };
    const lengths = [];
    let total = 0;
    for (let index = 1; index < points.length; index += 1) {
        const dx = points[index].x - points[index - 1].x;
        const dy = points[index].y - points[index - 1].y;
        const length = Math.hypot(dx, dy);
        lengths.push(length);
        total += length;
    }
    let target = Math.max(0, Math.min(1, progress)) * total;
    for (let index = 0; index < lengths.length; index += 1) {
        if (target <= lengths[index] || index === lengths.length - 1) {
            const start = points[index];
            const end = points[index + 1];
            const segmentProgress = lengths[index] === 0 ? 0 : Math.min(1, target / lengths[index]);
            return {
                x: start.x + (end.x - start.x) * segmentProgress,
                y: start.y + (end.y - start.y) * segmentProgress,
                angle: Math.atan2(end.y - start.y, end.x - start.x) * 180 / Math.PI
            };
        }
        target -= lengths[index];
    }
    return { x: points.at(-1).x, y: points.at(-1).y, angle: 0 };
}

async function approachRouteFor(booking) {
    if (!booking.rider) return null;
    const key = `${booking.rider.locationId}-${booking.pickup.id}`;
    if (state.approachRoutes.has(key)) return state.approachRoutes.get(key);
    try {
        const comparison = await getJson(`/api/route?from=${booking.rider.locationId}&to=${booking.pickup.id}`);
        state.approachRoutes.set(key, comparison.minimumDistance);
        return comparison.minimumDistance;
    } catch (_) {
        return null;
    }
}

function deliveryCopy(booking) {
    const origin = locationPoint(booking.rider?.locationId)?.name || "the rider location";
    switch (booking.status) {
        case "Rider Assigned": return ["Rider assigned", `${booking.rider?.name || "Your rider"} is preparing to leave ${origin}.`, "approach"];
        case "Rider Arriving": return ["Rider approaching pickup", `Moving from ${origin} to ${booking.pickup.name}.`, "approach"];
        case "Picked Up": return ["Parcel picked up", `Collection confirmed at ${booking.pickup.name}.`, "picked"];
        case "In Transit": return ["Parcel in transit", `Travelling toward ${booking.destination.name}.`, "transit"];
        case "Delivered": return ["Parcel delivered", `Delivered successfully at ${booking.destination.name}.`, "delivered"];
        default: return [booking.status, "Waiting for the next delivery update.", "approach"];
    }
}

function setDeliveryOverlay(booking, progress) {
    const [phase, detail, visual] = deliveryCopy(booking);
    elements.deliveryMapStatus.hidden = false;
    elements.deliveryMapStatus.classList.toggle("transit", visual === "transit");
    elements.deliveryMapStatus.classList.toggle("delivered", visual === "delivered" || visual === "picked");
    elements.deliveryMapStatus.querySelector(".delivery-status-icon")?.classList.toggle("transit", visual === "transit");
    elements.deliveryMapStatus.querySelector(".delivery-status-icon")?.classList.toggle("delivered", visual === "delivered" || visual === "picked");
    elements.deliveryMapTracking.textContent = booking.trackingId;
    elements.deliveryMapPhase.textContent = phase;
    elements.deliveryMapDetail.textContent = detail;
    elements.deliveryMapProgress.style.width = `${Math.round(progress * 100)}%`;
}

function appendDeliveryPin(location, className, label) {
    if (!location) return;
    const pin = svgElement("circle", { cx: location.x, cy: location.y, r: 12, class: `delivery-pin ${className}` });
    const title = svgElement("title");
    title.textContent = label;
    pin.append(title);
    elements.deliveryLayer.append(pin);
}

function createDeliveryVehicle(className) {
    const vehicle = svgElement("g", { class: `delivery-vehicle ${className}`.trim() });
    vehicle.append(
        svgElement("circle", { cx: 0, cy: 0, r: 24, class: "vehicle-halo" }),
        svgElement("circle", { cx: 0, cy: 0, r: 15, class: "vehicle-body" }),
        svgElement("path", { d: "M-8 2h6l3-7m-5 7-3 6m8-13 6 7H1m-9 6a3 3 0 1 0 0 .1M8 8a3 3 0 1 0 0 .1" })
    );
    elements.deliveryLayer.append(vehicle);
    return vehicle;
}

async function renderDeliveryJourney(booking, force = false) {
    if (!state.city || !booking?.rider || (!ACTIVE_DELIVERY_STATUSES.includes(booking.status) && booking.status !== "Delivered")) {
        window.cancelAnimationFrame(state.deliveryAnimationFrame);
        elements.deliveryLayer.replaceChildren();
        elements.deliveryMapStatus.hidden = true;
        state.renderedDeliveryKey = "";
        return;
    }

    const key = `${booking.trackingId}|${booking.status}|${booking.statusChangedAt}|${state.city.roads.length}`;
    if (!force && key === state.renderedDeliveryKey) return;
    state.renderedDeliveryKey = key;
    window.cancelAnimationFrame(state.deliveryAnimationFrame);
    elements.deliveryLayer.replaceChildren();

    const pickup = locationPoint(booking.pickup.id);
    const destination = locationPoint(booking.destination.id);
    appendDeliveryPin(pickup, "pickup", `Pickup: ${booking.pickup.name}`);
    appendDeliveryPin(destination, "destination", `Destination: ${booking.destination.name}`);

    let route = booking.route;
    let points = [];
    let visual = "transit";
    let animate = booking.status === "In Transit";
    if (booking.status === "Rider Assigned" || booking.status === "Rider Arriving") {
        route = await approachRouteFor(booking);
        points = routePoints(route);
        if (points.length < 2) points = [locationPoint(booking.rider.locationId), pickup].filter(Boolean);
        visual = "approach";
        animate = booking.status === "Rider Arriving";
    } else if (booking.status === "Picked Up") {
        points = [pickup].filter(Boolean);
        visual = "delivered";
        animate = false;
    } else if (booking.status === "Delivered") {
        points = [destination].filter(Boolean);
        visual = "delivered";
        animate = false;
    } else {
        points = routePoints(route);
    }

    if (points.length > 1) {
        const polyline = svgElement("polyline", {
            points: points.map(point => `${point.x},${point.y}`).join(" "),
            class: `delivery-route ${visual}`
        });
        elements.deliveryLayer.insertBefore(polyline, elements.deliveryLayer.firstChild);
    }
    const vehicle = createDeliveryVehicle(visual === "approach" ? "" : visual);

    const update = () => {
        const timedProgress = progressForBooking(booking);
        const movementProgress = animate ? timedProgress : (booking.status === "Delivered" ? 1 : 0);
        const point = interpolatePath(points, movementProgress);
        vehicle.setAttribute("transform", `translate(${point.x} ${point.y}) rotate(${point.angle})`);
        setDeliveryOverlay(booking, booking.status === "Delivered" ? 1 : timedProgress);
        if (state.renderedDeliveryKey === key && !FINAL_DELIVERY_STATUSES.includes(booking.status)) {
            state.deliveryAnimationFrame = window.requestAnimationFrame(update);
        }
    };
    update();
}

function currentMapBooking() {
    if (state.latestBooking && ACTIVE_DELIVERY_STATUSES.includes(state.latestBooking.status)) {
        return state.latestBooking;
    }
    const active = [...state.bookings].reverse().find(booking => ACTIVE_DELIVERY_STATUSES.includes(booking.status));
    if (active) return active;
    return state.latestBooking?.status === "Delivered" ? state.latestBooking : null;
}

function renderCurrentDelivery(force = false) {
    renderDeliveryJourney(currentMapBooking(), force);
}

function resetNewBookingFlow() {
    elements.routeForm.reset();
    elements.pickup.value = "";
    elements.destination.value = "";
    state.routeComparison = null;
    state.activeRouteMode = "minimumDistance";
    elements.routeResults.hidden = true;
    elements.foundationNote.hidden = false;
    elements.routeLayer.replaceChildren();
    document.querySelectorAll(".choice-card").forEach(card => card.classList.remove("selected"));
    const quick = document.querySelector('.choice-card input[value="quick"]');
    if (quick) {
        quick.checked = true;
        quick.closest(".choice-card").classList.add("selected");
    }
    elements.bookingDetailsForm.reset();
    byId("customer-name").value = "";
    byId("receiver-name").value = "";
    byId("receiver-phone").value = "";
    document.querySelector(".booking-card")?.scrollIntoView({ behavior: "smooth", block: "center" });
    window.setTimeout(() => elements.pickup.focus(), 420);
}

function openBookingDialog() {
    if (!state.routeComparison) {
        showToast("Calculate a route first", "Choose pickup and delivery locations, then compare the options.", true);
        return;
    }
    elements.bookingDetailsForm.reset();
    byId("customer-name").value = "";
    byId("receiver-name").value = "";
    byId("receiver-phone").value = "";
    const selectedRoute = state.routeComparison[state.activeRouteMode] || state.routeComparison.minimumDistance;
    byId("modal-pickup").textContent = state.routeComparison.from.name;
    byId("modal-destination").textContent = state.routeComparison.to.name;
    byId("modal-distance").textContent = `${selectedRoute.distance.toFixed(1)} km`;
    byId("modal-route-time").textContent = `${selectedRoute.estimatedMinutes.toFixed(1)} min`;
    byId("estimated-fare-preview").textContent = "Based on distance, weight, priority, and traffic";
    byId("booking-modal-title").textContent = "Complete your booking";
    elements.bookingDetailsView.hidden = false;
    elements.offerSelectionView.hidden = true;
    elements.confirmationView.hidden = true;
    openModal(elements.bookingModal);
    window.setTimeout(() => byId("customer-name").focus(), 100);
}

function buildOfferCard(offer) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "offer-card";
    const avatar = textElement("span", "rider-avatar", initials(offer.riderName));
    const details = document.createElement("div");
    details.append(
        textElement("strong", "", offer.riderName),
        textElement("small", "", `${offer.vehicle} · ${ratingLabel(offer.rating)} · ${offer.pickupDistance.toFixed(1)} km away`)
    );
    const price = document.createElement("div");
    price.className = "offer-price";
    price.append(textElement("span", "", `${offer.pickupMinutes.toFixed(0)} min pickup`), textElement("strong", "", money(offer.amount)));
    const arrow = svgElement("svg", { viewBox: "0 0 24 24" });
    arrow.append(svgElement("path", { d: "M5 12h14m-5-5 5 5-5 5" }));
    button.append(avatar, details, price, arrow);
    button.addEventListener("click", () => acceptRiderOffer(offer.riderId, button));
    return button;
}

function showOfferSelection(booking) {
    state.latestBooking = booking;
    elements.bookingDetailsView.hidden = true;
    elements.confirmationView.hidden = true;
    elements.offerSelectionView.hidden = false;
    byId("booking-modal-title").textContent = "Choose a rider offer";
    elements.offerList.replaceChildren(...booking.offers.map(buildOfferCard));
}

function showBookingConfirmation(booking) {
    state.latestBooking = booking;
    elements.bookingDetailsView.hidden = true;
    elements.offerSelectionView.hidden = true;
    elements.confirmationView.hidden = false;
    byId("booking-modal-title").textContent = "Booking confirmed";
    byId("confirmed-tracking-id").textContent = booking.trackingId;
    const rider = booking.rider;
    byId("confirmed-rider-avatar").textContent = initials(rider?.name || "Searching");
    byId("confirmed-rider-name").textContent = rider?.name || "Searching for rider";
    byId("confirmed-rider-detail").textContent = rider
        ? `${rider.vehicle} · ${rider.registration} · ${ratingLabel(rider.rating)}`
        : "The request is waiting for an available rider";
    byId("confirmed-fare").textContent = money(booking.acceptedFare);
    renderDeliveryJourney(booking, true);
}

async function acceptRiderOffer(riderId, button) {
    if (!state.latestBooking) return;
    button.disabled = true;
    try {
        const booking = await postForm("/api/bookings/accept-offer", { trackingId: state.latestBooking.trackingId, riderId });
        state.knownBookingStatuses.set(booking.trackingId, booking.status);
        showBookingConfirmation(booking);
        addNotification("Rider assigned", `${booking.rider.name} is preparing to collect ${booking.trackingId}.`, "rider", booking.trackingId, `${booking.trackingId}|assigned`);
        showToast("Offer accepted", `${booking.rider.name} is now assigned to the parcel.`);
        await pollLiveState();
    } catch (error) {
        button.disabled = false;
        showToast("Offer unavailable", error.message, true);
    }
}

async function submitBookingDetails(event) {
    event.preventDefault();
    if (!state.routeComparison) return;
    const submitButton = elements.bookingDetailsForm.querySelector(".modal-primary");
    submitButton.disabled = true;
    try {
        const selectedMode = document.querySelector('input[name="mode"]:checked')?.value || "quick";
        const booking = await postForm("/api/bookings", {
            customerName: byId("customer-name").value.trim(),
            receiverName: byId("receiver-name").value.trim(),
            receiverPhone: byId("receiver-phone").value.trim(),
            pickupId: state.routeComparison.from.id,
            destinationId: state.routeComparison.to.id,
            weightKg: byId("weight-input").value,
            priority: byId("priority-select").value,
            mode: selectedMode
        });
        state.latestBooking = booking;
        state.knownBookingStatuses.set(booking.trackingId, booking.status);
        addNotification("Parcel booked", `${booking.trackingId} was created for ${booking.destination.name}.`, "booking", booking.trackingId, `${booking.trackingId}|created`);
        if (booking.mode === "Rider Offers" && booking.offers.length > 0 && !booking.rider) showOfferSelection(booking);
        else showBookingConfirmation(booking);
        showToast("Parcel booked", `Tracking ID ${booking.trackingId} was created.`);
        await pollLiveState();
    } catch (error) {
        showToast("Booking failed", error.message, true);
    } finally {
        submitButton.disabled = false;
    }
}

function openTrackingDialog(trackingId = "") {
    closeModal(elements.bookingModal);
    openModal(elements.trackingModal);
    elements.trackingInput.value = trackingId;
    if (trackingId) loadTracking(trackingId);
    else {
        state.trackedBooking = null;
        elements.trackingEmpty.hidden = false;
        elements.trackingResult.hidden = true;
        window.setTimeout(() => elements.trackingInput.focus(), 100);
    }
}

async function loadTracking(trackingId) {
    try {
        const booking = await getJson(`/api/track?id=${encodeURIComponent(trackingId.trim())}`);
        renderTracking(booking);
    } catch (_) {
        elements.trackingEmpty.hidden = false;
        elements.trackingResult.hidden = true;
        showToast("Parcel not found", "Check the tracking ID and try again.", true);
    }
}

function updateTrackingCountdown(booking) {
    if (!booking || FINAL_DELIVERY_STATUSES.includes(booking.status) || !booking.nextStatusAt) {
        elements.automaticStatusCountdown.textContent = booking?.status === "Delivered"
            ? "Delivery completed successfully"
            : "No further automatic updates";
        return;
    }
    const seconds = Math.max(0, Math.ceil(booking.nextStatusAt - Date.now() / 1000));
    elements.automaticStatusCountdown.textContent = seconds > 0 ? `Next update in about ${seconds}s` : "Updating now…";
}

function renderTracking(booking) {
    state.trackedBooking = booking;
    elements.trackingEmpty.hidden = true;
    elements.trackingResult.hidden = false;
    elements.trackingInput.value = booking.trackingId;
    const status = byId("tracking-status");
    status.textContent = booking.status;
    status.className = `status-chip ${booking.status.toLowerCase().replaceAll(" ", "-")}`;
    byId("tracking-route-title").textContent = `${booking.pickup.name} to ${booking.destination.name}`;
    byId("tracking-recipient").textContent = `Receiver: ${booking.receiverName} · ${booking.trackingId}`;
    byId("tracking-fare").textContent = money(booking.acceptedFare);

    const riderBox = byId("tracking-rider");
    if (booking.rider) {
        riderBox.hidden = false;
        riderBox.querySelector(".rider-avatar").textContent = initials(booking.rider.name);
        riderBox.querySelector("strong").textContent = booking.rider.name;
        riderBox.querySelector("small").textContent = `${booking.rider.vehicle} · ${booking.rider.registration} · ${ratingLabel(booking.rider.rating)}`;
    } else riderBox.hidden = true;

    const ratingAvailable = booking.status === "Delivered" && Boolean(booking.rider);
    elements.deliveryRatingCard.hidden = !ratingAvailable;
    if (ratingAvailable) {
        const alreadyRated = Number(booking.customerRating) > 0;
        elements.deliveryRatingForm.hidden = alreadyRated;
        elements.deliveryRatingComplete.hidden = !alreadyRated;
        if (alreadyRated) {
            elements.deliveryRatingComplete.textContent =
                `You rated this delivery ${ratingLabel(booking.customerRating)}. Thank you for your feedback.`;
        } else {
            elements.deliveryRatingSelect.value = "";
        }
    }

    const timeline = byId("status-timeline");
    timeline.replaceChildren();
    booking.history.forEach((event, index) => {
        const row = document.createElement("div");
        row.className = "status-event";
        const details = document.createElement("div");
        details.append(textElement("strong", "", event.status), textElement("p", "", event.note));
        row.append(textElement("span", "", index + 1), details, textElement("time", "", event.time.split(" ").at(-1)));
        timeline.append(row);
    });

    const finalStatus = FINAL_DELIVERY_STATUSES.includes(booking.status);
    byId("cancel-booking-button").hidden = finalStatus || ["Picked Up", "In Transit"].includes(booking.status);
    elements.automaticDeliveryNote.hidden = finalStatus;
    updateTrackingCountdown(booking);
    renderDeliveryJourney(booking);
}

async function cancelTrackedBooking() {
    if (!state.trackedBooking) return;
    try {
        const booking = await postForm("/api/bookings/cancel", { trackingId: state.trackedBooking.trackingId });
        renderTracking(booking);
        addNotification("Booking cancelled", `${booking.trackingId} was cancelled.`, "booking", booking.trackingId, `${booking.trackingId}|cancelled`);
        showToast("Booking cancelled", `${booking.trackingId} has been cancelled.`);
        await pollLiveState();
    } catch (error) {
        showToast("Cancellation failed", error.message, true);
    }
}

async function submitDeliveryRating(event) {
    event.preventDefault();
    if (!state.trackedBooking) return;
    const button = elements.deliveryRatingForm.querySelector("button[type=submit]");
    button.disabled = true;
    try {
        const booking = await postForm("/api/bookings/rate", {
            trackingId: state.trackedBooking.trackingId,
            rating: elements.deliveryRatingSelect.value
        });
        state.trackedBooking = booking;
        state.bookings = state.bookings.map(item =>
            item.trackingId === booking.trackingId ? booking : item
        );
        renderTracking(booking);
        addNotification(
            "Delivery rated",
            `${ratingLabel(booking.customerRating)} feedback was saved for ${booking.rider?.name || "the rider"}.`,
            "success",
            booking.trackingId,
            `${booking.trackingId}|rating|${booking.customerRating}`
        );
        showToast("Rating submitted", "The rider's customer rating was updated.");
        await pollLiveState();
    } catch (error) {
        showToast("Rating could not be saved", error.message, true);
    } finally {
        button.disabled = false;
    }
}

function roadDisplayName(road) {
    const locations = new Map((state.city?.locations || []).map(location => [location.id, location.name]));
    return `${road.name || road.id} · ${locations.get(road.from) || road.from} to ${locations.get(road.to) || road.to}`;
}

function populateRoadControl() {
    if (!state.city) return;
    const selectedValue = elements.roadSelect.value;
    elements.roadSelect.innerHTML = state.city.roads.map(road => `<option value="${road.id}">${roadDisplayName(road)}</option>`).join("");
    if (state.city.roads.some(road => road.id === selectedValue)) elements.roadSelect.value = selectedValue;
    updateSelectedRoadDetails();
}

function selectedRoad() {
    return state.city?.roads.find(road => road.id === elements.roadSelect.value) || null;
}

function updateSelectedRoadDetails() {
    const road = selectedRoad();
    if (!road || !state.city) return;
    const locations = new Map(state.city.locations.map(location => [location.id, location.name]));
    byId("selected-road-id").textContent = road.id;
    byId("selected-road-name").textContent = road.name || `${locations.get(road.from)} to ${locations.get(road.to)}`;
    byId("selected-road-metric").textContent = `${road.distance.toFixed(1)} km · ${road.traffic} traffic`;
    const stateLabel = byId("selected-road-state");
    stateLabel.textContent = road.blocked ? "Blocked" : "Open";
    stateLabel.classList.toggle("blocked", road.blocked);
    elements.trafficSelect.value = road.traffic;
    const toggle = byId("toggle-road-button");
    toggle.classList.toggle("unblock", road.blocked);
    toggle.querySelector("strong").textContent = road.blocked ? "Unblock road" : "Block road";
    toggle.querySelector("small").textContent = road.blocked ? "Return it to active route planning" : "Remove it from active route planning";
}

function openRoadControl() {
    if (!state.adminSessionToken) {
        openAccess("admin");
        return;
    }
    populateRoadControl();
    openModal(elements.roadModal);
}

async function refreshAfterRoadChange(city, message) {
    const previousComparison = state.routeComparison;
    state.city = city;
    state.routeComparison = null;
    state.approachRoutes.clear();
    populateLocationSelects(city.locations);
    populateRoadControl();
    renderCityMap(city, state.riders);
    if (previousComparison) {
        try {
            showRouteComparison(await getJson(`/api/route?from=${encodeURIComponent(previousComparison.from.name)}&to=${encodeURIComponent(previousComparison.to.name)}`));
        } catch (_) {
            elements.routeResults.hidden = true;
            elements.foundationNote.hidden = false;
        }
    }
    showToast("Road network updated", message);
}

async function toggleSelectedRoad() {
    const road = selectedRoad();
    if (!road) return;
    try {
        const city = await postForm("/api/roads/block", { token: state.adminSessionToken, roadId: road.id, blocked: road.blocked ? "0" : "1" });
        await refreshAfterRoadChange(city, `${road.id} is now ${road.blocked ? "open" : "blocked"}.`);
        addNotification("Road availability changed", `${road.id} is now ${road.blocked ? "open" : "blocked"}.`, "road");
    } catch (error) {
        showToast("Road update failed", error.message, true);
    }
}

async function applyTrafficLevel() {
    const road = selectedRoad();
    if (!road) return;
    try {
        const traffic = elements.trafficSelect.value;
        const city = await postForm("/api/roads/traffic", { token: state.adminSessionToken, roadId: road.id, traffic });
        await refreshAfterRoadChange(city, `${road.id} traffic changed to ${traffic}.`);
        addNotification("Traffic setting updated", `${road.id} is now marked as ${traffic.toLowerCase()} traffic.`, "road");
    } catch (error) {
        showToast("Traffic update failed", error.message, true);
    }
}

async function undoRoadChange() {
    try {
        const city = await postForm("/api/roads/undo", { token: state.adminSessionToken });
        await refreshAfterRoadChange(city, "The latest road change was restored.");
        addNotification("Road change restored", "The most recent network setting was undone.", "road");
    } catch (error) {
        showToast("Nothing to undo", error.message, true);
    }
}

function operationSummary(items) {
    const summary = document.createElement("div");
    summary.className = "operations-summary";
    items.forEach(item => {
        const card = document.createElement("article");
        card.className = "operation-stat";
        card.append(textElement("span", "", item.label), textElement("strong", "", item.value), textElement("small", "", item.note));
        summary.append(card);
    });
    return summary;
}

function operationHeading(title, detail) {
    const heading = document.createElement("div");
    heading.className = "operation-section-heading";
    heading.append(textElement("strong", "", title), textElement("span", "", detail));
    return heading;
}

function openOperationsShell(eyebrow, title) {
    byId("operations-eyebrow").textContent = eyebrow;
    byId("operations-modal-title").textContent = title;
    elements.operationsContent.innerHTML = '<div class="operations-loading"><span></span><p>Loading live service data</p></div>';
    openModal(elements.operationsModal);
}

function showOperationsError(error) {
    elements.operationsContent.replaceChildren();
    const empty = document.createElement("div");
    empty.className = "report-empty";
    empty.append(textElement("strong", "", "The module could not load"), textElement("p", "", error.message || "Check that the local service is running and try again."));
    elements.operationsContent.append(empty);
}

async function openRiderDirectory() {
    openOperationsShell("Fleet management", "Nexa City rider directory");
    try {
        await loadAdminData();
        const riders = state.adminRiders;
        const available = riders.filter(rider => rider.available).length;
        const completed = riders.reduce((sum, rider) => sum + rider.completed, 0);
        const ratedRiders = riders.filter(rider => Number(rider.rating) > 0);
        const averageRating = ratedRiders.length
            ? ratedRiders.reduce((sum, rider) => sum + rider.rating, 0) / ratedRiders.length
            : 0;
        elements.operationsContent.replaceChildren();
        elements.operationsContent.append(operationSummary([
            { label: "Registered", value: riders.length, note: "Total fleet profiles" },
            { label: "Available", value: available, note: "Ready for matching" },
            { label: "Completed", value: completed, note: "Fleet deliveries" },
            {
                label: "Avg. rating",
                value: ratedRiders.length ? averageRating.toFixed(1) : "Not rated",
                note: ratedRiders.length ? `${ratedRiders.length} rated profile${ratedRiders.length === 1 ? "" : "s"}` : "No customer ratings yet"
            }
        ]));
        elements.operationsContent.append(operationHeading("Live rider roster", `${available} currently available`));
        const locations = new Map((state.city?.locations || []).map(location => [location.id, location.name]));
        const roster = document.createElement("div");
        roster.className = "rider-roster";
        if (riders.length === 0) {
            const empty = document.createElement("div");
            empty.className = "report-empty";
            empty.append(
                textElement("strong", "", "No riders registered"),
                textElement("p", "", "New rider profiles will appear here after registration.")
            );
            elements.operationsContent.append(empty);
            return;
        }
        riders.forEach(rider => {
            const card = document.createElement("article");
            card.className = "rider-directory-card";
            const details = document.createElement("div");
            details.append(
                textElement("strong", "", `${rider.name} · ${ratingLabel(rider.rating)}`),
                textElement("small", "", `${rider.vehicle} · ${rider.registration}`),
                textElement("small", "", `${locations.get(rider.locationId) || "Unknown location"} · ${money(rider.baseCharge)} + ${money(rider.perKmCharge)}/km`)
            );
            card.append(
                textElement("span", "rider-avatar", initials(rider.name)),
                details,
                textElement("span", `availability-pill${rider.available ? "" : " busy"}`, rider.available ? "Available" : "On delivery")
            );
            roster.append(card);
        });
        elements.operationsContent.append(roster);
    } catch (error) {
        showOperationsError(error);
    }
}

async function openDeliveryHistory() {
    openOperationsShell("Booking records", "Delivery history");
    try {
        await loadAdminData();
        const bookings = state.adminBookings;
        const dashboard = state.adminDashboard;
        elements.operationsContent.replaceChildren();
        elements.operationsContent.append(operationSummary([
            { label: "All bookings", value: dashboard.totalBookings, note: "Saved parcel requests" },
            { label: "Delivered", value: dashboard.deliveredBookings, note: "Completed parcels" },
            { label: "Active", value: dashboard.activeDeliveries, note: "Moving parcels" },
            { label: "Revenue", value: money(dashboard.totalRevenue), note: "Delivered value" }
        ]));
        elements.operationsContent.append(operationHeading("Booking records", "Select a row to open tracking"));
        if (bookings.length === 0) {
            const empty = document.createElement("div");
            empty.className = "report-empty";
            empty.append(textElement("strong", "", "No bookings yet"), textElement("p", "", "Create a parcel booking and it will appear here."));
            elements.operationsContent.append(empty);
            return;
        }
        const log = document.createElement("div");
        log.className = "booking-log";
        [...bookings].reverse().forEach(booking => {
            const row = document.createElement("button");
            row.type = "button";
            row.className = "booking-log-row";
            const identity = document.createElement("div");
            identity.append(textElement("strong", "", booking.trackingId), textElement("small", "", `${booking.customerName} to ${booking.receiverName}`));
            const route = document.createElement("div");
            route.className = "booking-log-route";
            route.append(textElement("strong", "", `${booking.pickup.name} → ${booking.destination.name}`), textElement("small", "", `${booking.priority} · ${booking.weightKg.toFixed(1)} kg`));
            const price = document.createElement("div");
            price.className = "booking-log-price";
            price.append(textElement("strong", "", money(booking.acceptedFare)), textElement("small", "", booking.status));
            row.append(textElement("span", "parcel-symbol", "↗"), identity, route, price);
            row.addEventListener("click", () => {
                closeModal(elements.operationsModal);
                setActiveRole("customer");
                openTrackingDialog(booking.trackingId);
            });
            log.append(row);
        });
        elements.operationsContent.append(log);
    } catch (error) {
        showOperationsError(error);
    }
}

async function openReports() {
    openOperationsShell("Business performance", "Operations reports");
    try {
        await loadAdminData();
        const dashboard = state.adminDashboard;
        const city = state.city;
        const analytics = state.adminAnalytics;
        const total = Math.max(1, dashboard.totalBookings);
        const deliveryRate = dashboard.totalBookings ? dashboard.deliveredBookings / dashboard.totalBookings * 100 : 0;
        const fleetReady = analytics?.fleet?.availabilityPercentage || 0;
        const margin = analytics?.finance?.profitMarginPercentage || 0;
        elements.operationsContent.replaceChildren();
        elements.operationsContent.append(operationSummary([
            { label: "Gross revenue", value: money(dashboard.totalRevenue), note: "Completed bookings" },
            { label: "Rider payouts", value: money(dashboard.riderPayouts), note: "Fleet earnings share" },
            { label: "Platform profit", value: money(dashboard.platformProfit), note: `${margin.toFixed(0)}% net share` },
            { label: "Delivery rate", value: `${deliveryRate.toFixed(0)}%`, note: "Completed of all bookings" }
        ]));
        elements.operationsContent.append(operationHeading("Network health", "Current operational indicators"));
        const metrics = [
            ["F", "Fleet readiness", `${fleetReady.toFixed(0)}% of registered riders are available.`],
            ["A", "Active parcels", `${dashboard.activeDeliveries} deliveries are progressing automatically.`],
            ["N", "Network coverage", `${analytics?.network?.reachableLocations || 0} of ${city.locations.length} locations are currently reachable.`],
            ["C", "Cancellations", `${dashboard.cancelledBookings} bookings were cancelled before pickup.`]
        ];
        const grid = document.createElement("div");
        grid.className = "report-metric-grid";
        metrics.forEach(([icon, title, copy]) => {
            const card = document.createElement("article");
            card.className = "report-metric-card";
            card.append(textElement("i", "", icon), textElement("strong", "", title), textElement("p", "", copy));
            grid.append(card);
        });
        elements.operationsContent.append(grid);
        const bars = document.createElement("div");
        bars.className = "report-bars";
        [["Delivered", dashboard.deliveredBookings], ["Active", dashboard.activeDeliveries], ["Pending", dashboard.pendingBookings], ["Cancelled", dashboard.cancelledBookings]].forEach(([label, value]) => {
            const row = document.createElement("div");
            row.className = "report-bar";
            const track = textElement("span", "report-bar-track", "");
            const fill = document.createElement("i");
            fill.style.width = `${Math.max(value > 0 ? 5 : 0, value / total * 100)}%`;
            track.append(fill);
            row.append(textElement("span", "", label), track, textElement("strong", "", `${value} / ${dashboard.totalBookings}`));
            bars.append(row);
        });
        elements.operationsContent.append(bars);
    } catch (error) {
        showOperationsError(error);
    }
}

function selectedRider() {
    return state.authenticatedRider;
}

function renderRiderJob(rider) {
    elements.riderActiveJob.replaceChildren();
    const booking = rider?.activeTrackingId ? state.bookings.find(item => item.trackingId === rider.activeTrackingId) : null;
    if (!booking) {
        const empty = document.createElement("div");
        empty.className = "rider-profile-empty";
        empty.append(textElement("span", "", "✓"), textElement("strong", "", "No active delivery"), textElement("p", "", rider ? "You are ready for a new parcel assignment." : "Login to view your assignments."));
        elements.riderActiveJob.append(empty);
        return;
    }
    const detail = document.createElement("div");
    detail.className = "rider-job-detail";
    const route = document.createElement("div");
    route.className = "rider-job-route";
    const pickup = document.createElement("div");
    pickup.append(textElement("span", "", "Pickup"), textElement("strong", "", booking.pickup.name));
    const arrow = svgElement("svg", { viewBox: "0 0 24 24" });
    arrow.append(svgElement("path", { d: "M5 12h14m-5-5 5 5-5 5" }));
    const destination = document.createElement("div");
    destination.append(textElement("span", "", "Destination"), textElement("strong", "", booking.destination.name));
    route.append(pickup, arrow, destination);
    const meta = document.createElement("div");
    meta.className = "rider-job-meta";
    [["Tracking", booking.trackingId], ["Status", booking.status], ["Rider share", money(booking.acceptedFare * 0.78)]].forEach(([label, value]) => {
        const item = document.createElement("div");
        item.append(textElement("span", "", label), textElement("strong", "", value));
        meta.append(item);
    });
    detail.append(route, meta);
    elements.riderActiveJob.append(detail);
}

function renderRiderPanel() {
    const rider = selectedRider();
    byId("rider-selected-id").textContent = rider?.id || "Signed out";
    byId("rider-selected-name").textContent = rider?.name || "Login required";
    byId("rider-panel-status").textContent = rider
        ? (rider.available ? "Available" : rider.activeTrackingId ? "Busy" : "Offline")
        : "Offline";
    byId("rider-panel-job").textContent = rider?.activeTrackingId || (rider ? "Ready for assignments" : "No account selected");
    byId("rider-panel-earnings").textContent = money(rider?.earnings || 0);
    byId("rider-panel-completed").textContent = rider?.completed || 0;
    byId("rider-panel-rating").textContent = rider
        ? (Number(rider.rating) > 0 ? `${ratingLabel(rider.rating)} customer rating` : "Not rated yet")
        : "Rating unavailable";
    if (rider) {
        byId("rider-profile-avatar").textContent = initials(rider.name);
        byId("rider-profile-name").textContent = `${rider.name} · ${rider.id}`;
        byId("rider-profile-vehicle").textContent = `${rider.vehicle} · ${rider.registration} · ${rider.phone}`;
        const pill = byId("rider-profile-pill");
        pill.textContent = rider.available ? "Available" : rider.activeTrackingId ? "On delivery" : "Offline";
        pill.classList.toggle("busy", !rider.available);
        byId("rider-availability").value = rider.available ? "1" : "0";
        byId("rider-availability").disabled = Boolean(rider.activeTrackingId);
        byId("rider-current-location").value = String(rider.locationId);
        byId("rider-current-location").disabled = Boolean(rider.activeTrackingId);
        byId("rider-base-charge").value = rider.baseCharge;
        byId("rider-per-km-charge").value = rider.perKmCharge;
    }
    renderRiderJob(rider);
}

async function updateRiderProfile(event) {
    event.preventDefault();
    const rider = selectedRider();
    if (!rider) return;
    const submit = elements.riderProfileForm.querySelector("button[type=submit]");
    submit.disabled = true;
    try {
        const payload = await postForm("/api/riders/update", {
            token: state.riderSessionToken,
            riderId: rider.id,
            available: byId("rider-availability").value,
            locationId: byId("rider-current-location").value,
            baseCharge: byId("rider-base-charge").value,
            perKmCharge: byId("rider-per-km-charge").value
        });
        state.authenticatedRider = payload.rider;
        renderRiderPanel();
        const locationName = locationPoint(payload.rider.locationId)?.name || "the selected location";
        showToast("Rider profile saved", `Availability, location (${locationName}), and charges were updated.`);
    } catch (error) {
        showToast("Profile update failed", error.message, true);
    } finally {
        submit.disabled = false;
    }
}

async function registerRider(event) {
    event.preventDefault();
    const submit = elements.riderRegistrationForm.querySelector("button[type=submit]");
    submit.disabled = true;
    try {
        const payload = await postForm("/api/riders/register", {
            name: byId("new-rider-name").value.trim(),
            phone: byId("new-rider-phone").value.trim(),
            vehicle: byId("new-rider-vehicle").value,
            registration: byId("new-rider-registration").value.trim(),
            locationId: byId("new-rider-location").value,
            baseCharge: byId("new-rider-base").value,
            perKmCharge: byId("new-rider-per-km").value,
            password: byId("new-rider-password").value
        });
        state.riders.push(payload.rider);
        state.authenticatedRider = payload.rider;
        state.riderSessionToken = payload.session.token;
        storeSessionText("nexaroute-rider-session", state.riderSessionToken);
        elements.riderRegistrationForm.reset();
        closeModal(elements.accessModal);
        setActiveRole("rider");
        addNotification("Rider registered", `${payload.rider.name} joined with rider ID ${payload.rider.id}.`, "rider");
        showToast("Rider account created", `Your secure rider ID is ${payload.rider.id}.`);
    } catch (error) {
        showToast("Registration failed", error.message, true);
    } finally {
        submit.disabled = false;
    }
}

function renderAdminRecentBookings() {
    elements.adminRecentBookings.replaceChildren();
    const recent = [...state.adminBookings].reverse().slice(0, 5);
    if (recent.length === 0) {
        const empty = document.createElement("div");
        empty.className = "rider-profile-empty";
        empty.append(textElement("span", "", "↗"), textElement("strong", "", "No booking activity"), textElement("p", "", "New parcel requests will appear here."));
        elements.adminRecentBookings.append(empty);
        return;
    }
    recent.forEach(booking => {
        const row = document.createElement("button");
        row.type = "button";
        row.className = "compact-booking-row";
        const detail = document.createElement("div");
        detail.append(textElement("strong", "", `${booking.trackingId} · ${booking.receiverName}`), textElement("small", "", `${booking.pickup.name} → ${booking.destination.name}`));
        row.append(textElement("span", "parcel-symbol", "↗"), detail, textElement("span", "", booking.status));
        row.addEventListener("click", () => {
            setActiveRole("customer");
            openTrackingDialog(booking.trackingId);
        });
        elements.adminRecentBookings.append(row);
    });
}

function renderAdminPanel() {
    const dashboard = state.adminDashboard || {};
    byId("admin-total-bookings").textContent = dashboard.totalBookings || 0;
    byId("admin-booking-summary").textContent = `${dashboard.deliveredBookings || 0} delivered · ${dashboard.pendingBookings || 0} pending`;
    byId("admin-active-deliveries").textContent = dashboard.activeDeliveries || 0;
    byId("admin-revenue").textContent = money(dashboard.totalRevenue);
    byId("admin-profit").textContent = money(dashboard.platformProfit);
    const margin = dashboard.totalRevenue ? dashboard.platformProfit / dashboard.totalRevenue * 100 : 0;
    byId("profit-ring-value").textContent = `${margin.toFixed(0)}%`;
    byId("profit-ring").style.background = `radial-gradient(circle at center, #fff 0 57%, transparent 58%), conic-gradient(#7c3aed 0 ${margin}%, #dce5f5 ${margin}% 100%)`;
    byId("profit-gross").textContent = money(dashboard.totalRevenue);
    byId("profit-payout").textContent = money(dashboard.riderPayouts);
    byId("profit-net").textContent = money(dashboard.platformProfit);
    renderAdminRecentBookings();
}

async function addAdminRoad(event) {
    event.preventDefault();
    if (byId("admin-road-from").value === byId("admin-road-to").value) {
        showToast("Choose different locations", "A road must connect two different locations.", true);
        return;
    }
    const submit = elements.adminAddRoadForm.querySelector("button[type=submit]");
    submit.disabled = true;
    try {
        const routeName = byId("admin-road-name").value.trim();
        const city = await postForm("/api/roads/add", {
            token: state.adminSessionToken,
            routeName,
            from: byId("admin-road-from").value,
            to: byId("admin-road-to").value,
            distance: byId("admin-road-distance").value,
            baseTime: byId("admin-road-time").value,
            traffic: byId("admin-road-traffic").value
        });
        elements.adminAddRoadForm.reset();
        await refreshAfterRoadChange(city, `${routeName} was added to the city network.`);
        addNotification("New route added", `${routeName} is now available for delivery planning.`, "road");
        await loadAdminData();
        renderAdminPanel();
    } catch (error) {
        showToast("Road could not be added", error.message, true);
    } finally {
        submit.disabled = false;
    }
}

async function addAdminLocation(event) {
    event.preventDefault();
    const submit = elements.adminAddLocationForm.querySelector("button[type=submit]");
    submit.disabled = true;
    try {
        const locationName = byId("admin-location-name").value.trim();
        const routeName = byId("admin-location-route-name").value.trim();
        const city = await postForm("/api/locations/add", {
            token: state.adminSessionToken,
            locationName,
            locationType: byId("admin-location-type").value.trim(),
            connectFrom: byId("admin-location-anchor").value,
            routeName,
            distance: byId("admin-location-distance").value,
            baseTime: byId("admin-location-time").value,
            traffic: byId("admin-location-traffic").value
        });
        elements.adminAddLocationForm.reset();
        await refreshAfterRoadChange(city, `${locationName} was created and connected by ${routeName}.`);
        addNotification("Destination created", `${locationName} is now a saved delivery location.`, "road");
        await loadAdminData();
        renderAdminPanel();
    } catch (error) {
        showToast("Destination could not be created", error.message, true);
    } finally {
        submit.disabled = false;
    }
}

function updateDashboardCards() {
    const dashboard = state.dashboard;
    if (!dashboard) return;
    const available = state.riders.filter(rider => rider.available).length;
    elements.availableRiders.textContent = available;
    elements.riderSummary.textContent = state.riders.length
        ? `${state.riders.length} rider${state.riders.length === 1 ? "" : "s"} registered`
        : "No riders registered";
    elements.totalBookings.textContent = dashboard.totalBookings;
    elements.bookingSummary.textContent = `${dashboard.deliveredBookings} delivered · ${dashboard.pendingBookings} pending`;
    elements.activeDeliveries.textContent = dashboard.activeDeliveries;
    elements.deliverySummary.textContent = dashboard.activeDeliveries === 0 ? "Network standing by" : "Live deliveries in progress";
    if (state.activeRole === "rider") renderRiderPanel();
    if (state.activeRole === "admin") renderAdminPanel();
}

function statusNotification(booking) {
    const messages = {
        "Rider Assigned": ["Rider assigned", `${booking.rider?.name || "A rider"} is preparing to collect your parcel.`, "rider"],
        "Rider Arriving": ["Rider is approaching", `${booking.rider?.name || "Your rider"} is travelling to ${booking.pickup.name}.`, "delivery"],
        "Picked Up": ["Parcel picked up", `Collection was confirmed at ${booking.pickup.name}.`, "success"],
        "In Transit": ["Parcel is in transit", `Your parcel is moving toward ${booking.destination.name}.`, "delivery"],
        "Delivered": ["Parcel delivered", `${booking.trackingId} reached ${booking.destination.name}.`, "success"],
        "Cancelled": ["Booking cancelled", `${booking.trackingId} will not continue to delivery.`, "booking"]
    };
    return messages[booking.status] || ["Booking updated", `${booking.trackingId} is now ${booking.status}.`, "booking"];
}

function syncBookings(bookings, initial = false) {
    bookings.forEach(booking => {
        const previous = state.knownBookingStatuses.get(booking.trackingId);
        if (!initial && previous && previous !== booking.status) {
            const [title, message, type] = statusNotification(booking);
            addNotification(title, message, type, booking.trackingId, `${booking.trackingId}|${booking.status}|${booking.statusChangedAt}`);
            showToast(title, message);
        }
        state.knownBookingStatuses.set(booking.trackingId, booking.status);
    });
    state.bookings = bookings;
    if (!state.latestBooking && bookings.length) state.latestBooking = bookings.at(-1);
    else if (state.latestBooking) {
        state.latestBooking = bookings.find(item => item.trackingId === state.latestBooking.trackingId) || state.latestBooking;
    }
    if (state.trackedBooking) {
        const updated = bookings.find(item => item.trackingId === state.trackedBooking.trackingId);
        if (updated) renderTracking(updated);
    }
    renderCurrentDelivery();
    state.bookingSyncReady = true;
}

async function pollLiveState() {
    if (state.pollBusy || document.hidden) return;
    state.pollBusy = true;
    try {
        const [bookingPayload, riderPayload, dashboard] = await Promise.all([
            getJson("/api/bookings"),
            getJson("/api/riders"),
            getJson("/api/dashboard")
        ]);
        state.riders = riderPayload.riders || [];
        state.dashboard = dashboard;
        if (state.riderSessionToken) {
            try {
                const profile = await getJson(`/api/rider/profile?token=${encodeURIComponent(state.riderSessionToken)}`);
                state.authenticatedRider = profile.rider;
            } catch (_) {
                state.riderSessionToken = "";
                state.authenticatedRider = null;
                storeSessionText("nexaroute-rider-session", "");
                if (state.activeRole === "rider") setActiveRole("customer");
            }
        }
        if (state.activeRole === "admin" && state.adminSessionToken) {
            try { await loadAdminData(); } catch (_) {
                state.adminSessionToken = "";
                storeSessionText("nexaroute-admin-session", "");
                setActiveRole("customer");
            }
        }
        syncBookings(bookingPayload.bookings || [], !state.bookingSyncReady);
        updateDashboardCards();
        updateTrackingCountdown(state.trackedBooking);
        setConnection(true, "Service connected");
        state.pollFailures = 0;
    } catch (error) {
        state.pollFailures += 1;
        if (state.pollFailures >= 3) setConnection(false, "Service unavailable");
        console.error(error);
    } finally {
        state.pollBusy = false;
    }
}

async function loadDashboard() {
    try {
        const [health, city, riderPayload, dashboard, bookingPayload] = await Promise.all([
            getJson("/api/health"),
            getJson("/api/city"),
            getJson("/api/riders"),
            getJson("/api/dashboard"),
            getJson("/api/bookings")
        ]);
        state.city = city;
        state.riders = riderPayload.riders || [];
        state.dashboard = dashboard;
        populateLocationSelects(city.locations, false);
        populateRoadControl();
        syncBookings(bookingPayload.bookings || [], true);
        renderCityMap(city, state.riders);
        await restoreSessions();
        updateDashboardCards();
        setConnection(health.status === "ok", "Service connected");
        if (state.notifications.length === 0) {
            addNotification("NexaRoute is ready", "Book a parcel to receive live pickup and delivery updates here.", "success");
        } else renderNotifications();
        setActiveRole("customer");
        window.clearInterval(state.pollTimer);
        state.pollTimer = window.setInterval(pollLiveState, 1000);
    } catch (error) {
        console.error(error);
        setConnection(false, "Service unavailable");
        elements.mapLoading.querySelector("p").textContent = "Could not reach the local service";
        showToast("Connection failed", "Start the project, then refresh this page.", true);
    }
}

function handleNavigation(button) {
    document.querySelectorAll(`.nav-item[data-role-nav="${state.activeRole}"]`).forEach(item => item.classList.remove("active"));
    button.classList.add("active");
    const view = button.dataset.view;
    if (view === "overview" || view === "rider-dashboard" || view === "admin-dashboard") window.scrollTo({ top: 0, behavior: "smooth" });
    else if (view === "booking") resetNewBookingFlow();
    else if (view === "tracking") openTrackingDialog(state.latestBooking?.trackingId || "");
    else if (view === "rider-jobs") byId("rider-jobs-section").scrollIntoView({ behavior: "smooth", block: "center" });
    else if (view === "rider-logout") logout("rider");
    else if (view === "roads") openRoadControl();
    else if (view === "riders") openRiderDirectory();
    else if (view === "history") openDeliveryHistory();
    else if (view === "reports") openReports();
}

function configureInteractions() {
    document.querySelectorAll("[data-role-target]").forEach(button => button.addEventListener("click", () => requestWorkspace(button.dataset.roleTarget)));
    document.querySelectorAll(".nav-item").forEach(button => button.addEventListener("click", () => handleNavigation(button)));
    document.querySelectorAll(".notification-trigger").forEach(button => button.addEventListener("click", openNotifications));
    byId("mark-notifications-read").addEventListener("click", markNotificationsRead);

    document.querySelectorAll(".choice-card input").forEach(input => {
        input.addEventListener("change", () => {
            document.querySelectorAll(".choice-card").forEach(card => card.classList.remove("selected"));
            input.closest(".choice-card").classList.add("selected");
        });
    });

    document.querySelectorAll(".segmented-control button").forEach(button => {
        button.addEventListener("click", () => {
            document.querySelectorAll(".segmented-control button").forEach(item => item.classList.remove("active"));
            button.classList.add("active");
            const distanceView = button.textContent.trim() === "Distance";
            elements.cityMap.classList.toggle("distance-view", distanceView);
            showToast(`${button.textContent.trim()} view`, distanceView ? "Road lengths are emphasized across the city." : "Current traffic and road availability are emphasized.");
        });
    });

    elements.routeForm.addEventListener("submit", async event => {
        event.preventDefault();
        const pickupLocation = locationFromInput(elements.pickup.value);
        const destinationLocation = locationFromInput(elements.destination.value);
        if (!pickupLocation || !destinationLocation) {
            showToast("Saved locations required", "Type a saved location name or choose one from the suggestions.", true);
            return;
        }
        if (pickupLocation.id === destinationLocation.id) {
            showToast("Choose another destination", "Pickup and delivery locations must be different.", true);
            return;
        }
        const submitButton = elements.routeForm.querySelector(".route-button");
        const submitLabel = submitButton.querySelector("span");
        const originalLabel = submitLabel.textContent;
        submitButton.disabled = true;
        submitLabel.textContent = "Finding best routes…";
        try {
            elements.pickup.value = pickupLocation.name;
            elements.destination.value = destinationLocation.name;
            const comparison = await getJson(`/api/route?from=${encodeURIComponent(pickupLocation.name)}&to=${encodeURIComponent(destinationLocation.name)}`);
            showRouteComparison(comparison);
            const saved = comparison.minimumStops.distance - comparison.minimumDistance.distance;
            showToast("Routes ready", saved > 0 ? `The shortest option saves ${saved.toFixed(1)} km.` : "Both options have the same travel distance.");
        } catch (error) {
            showToast("Route calculation failed", error.message || "Try another pair of locations.", true);
        } finally {
            submitButton.disabled = false;
            submitLabel.textContent = originalLabel;
        }
    });

    document.querySelectorAll(".route-option").forEach(option => {
        option.addEventListener("click", () => {
            if (!state.routeComparison) return;
            state.activeRouteMode = option.dataset.routeMode;
            document.querySelectorAll(".route-option").forEach(item => item.classList.remove("selected"));
            option.classList.add("selected");
            drawRoute(state.routeComparison[state.activeRouteMode], state.activeRouteMode);
        });
    });

    byId("book-route-button").addEventListener("click", openBookingDialog);
    elements.bookingDetailsForm.addEventListener("submit", submitBookingDetails);
    elements.trackingSearchForm.addEventListener("submit", event => {
        event.preventDefault();
        loadTracking(elements.trackingInput.value);
    });
    byId("cancel-booking-button").addEventListener("click", cancelTrackedBooking);
    elements.deliveryRatingForm.addEventListener("submit", submitDeliveryRating);
    elements.roadSelect.addEventListener("change", updateSelectedRoadDetails);
    byId("toggle-road-button").addEventListener("click", toggleSelectedRoad);
    byId("apply-traffic-button").addEventListener("click", applyTrafficLevel);
    byId("undo-road-button").addEventListener("click", undoRoadChange);
    byId("open-tracking-button").addEventListener("click", () => openTrackingDialog(state.latestBooking?.trackingId || ""));
    byId("copy-tracking-button").addEventListener("click", async () => {
        if (!state.latestBooking) return;
        try {
            await navigator.clipboard.writeText(state.latestBooking.trackingId);
            showToast("Tracking ID copied", state.latestBooking.trackingId);
        } catch (_) {
            showToast("Tracking ID", state.latestBooking.trackingId);
        }
    });

    document.querySelectorAll("[data-close-modal]").forEach(button => button.addEventListener("click", () => closeModal(button.closest(".modal"))));
    document.addEventListener("keydown", event => {
        if (event.key !== "Escape") return;
        const open = [...document.querySelectorAll(".modal")].reverse().find(modal => !modal.hidden);
        if (open) closeModal(open);
    });

    byId("refresh-map").addEventListener("click", async () => {
        elements.mapLoading.classList.remove("hidden");
        await loadDashboard();
    });
    byId("new-booking-button").addEventListener("click", resetNewBookingFlow);

    document.querySelectorAll("[data-auth-view]").forEach(button => button.addEventListener("click", () => showAuthView(button.dataset.authView)));
    elements.riderLoginForm.addEventListener("submit", loginRider);
    elements.adminLoginForm.addEventListener("submit", loginAdmin);
    elements.riderProfileForm.addEventListener("submit", updateRiderProfile);
    elements.riderRegistrationForm.addEventListener("submit", registerRider);
    byId("rider-logout-button").addEventListener("click", () => logout("rider"));

    elements.adminAddRoadForm.addEventListener("submit", addAdminRoad);
    elements.adminAddLocationForm.addEventListener("submit", addAdminLocation);
    byId("admin-logout-button").addEventListener("click", () => logout("admin"));
    byId("admin-open-road-control").addEventListener("click", openRoadControl);
    byId("view-all-history").addEventListener("click", openDeliveryHistory);
    document.querySelectorAll("[data-admin-action]").forEach(button => button.addEventListener("click", () => {
        const action = button.dataset.adminAction;
        if (action === "roads") openRoadControl();
        else if (action === "riders") openRiderDirectory();
        else if (action === "history") openDeliveryHistory();
        else if (action === "reports") openReports();
    }));

    document.addEventListener("visibilitychange", () => { if (!document.hidden) pollLiveState(); });
    window.addEventListener("beforeunload", () => {
        window.clearInterval(state.pollTimer);
        window.cancelAnimationFrame(state.deliveryAnimationFrame);
    });
}

function setDate() {
    const formatted = new Intl.DateTimeFormat("en-PK", { weekday: "short", day: "2-digit", month: "short", year: "numeric" }).format(new Date());
    elements.currentDate.textContent = formatted;
    document.querySelectorAll(".shared-date").forEach(item => { item.textContent = formatted; });
}

setDate();
renderNotifications();
configureInteractions();
loadDashboard();
