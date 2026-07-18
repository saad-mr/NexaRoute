#!/usr/bin/env bash
set -euo pipefail

PORT=8097
SERVER="./build/SmartRouteOptimizer"
LOG="./build/smoke_server.log"

"$SERVER" --no-browser --no-persistence --no-automation --port "$PORT" >"$LOG" 2>&1 &
SERVER_PID=$!

cleanup() {
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

READY=0
for _ in $(seq 1 30); do
    if curl -fsS "http://127.0.0.1:$PORT/api/health" >/dev/null 2>&1; then
        READY=1
        break
    fi
    sleep 0.1
done

if [[ "$READY" != "1" ]]; then
    echo "Server did not become ready."
    sed -n '1,120p' "$LOG"
    exit 1
fi

curl -fsS "http://127.0.0.1:$PORT/api/health" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.status!=="ok"||j.version!=="3.1.0")process.exit(1);
 console.log("Health API: C++ services ready");
})'

curl -fsS "http://127.0.0.1:$PORT/api/city" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.locations.length!==15||j.roads.length!==32||!j.roads[0].name)process.exit(1);
 console.log("City API: locations, roads, and route names confirmed");
})'

curl -fsS "http://127.0.0.1:$PORT/api/riders" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.riders.length!==0)process.exit(1);
 console.log("Public rider API: clean fleet starts empty");
})'

curl -fsS --get "http://127.0.0.1:$PORT/api/route" \
    --data-urlencode "from=Airport Terminal" \
    --data-urlencode "to=Industrial Zone" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.minimumStops.distance!==18||j.minimumDistance.distance!==10)process.exit(1);
 console.log("Typed-name route API: both route options confirmed");
})'

UNAUTHORIZED_STATUS=$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/api/admin/dashboard")
if [[ "$UNAUTHORIZED_STATUS" != "401" ]]; then
    echo "Admin endpoint was not protected."
    exit 1
fi
echo "Admin protection API: unauthorized request rejected"

ADMIN_LOGIN=$(curl -fsS -X POST "http://127.0.0.1:$PORT/api/auth/admin/login" \
    --data-urlencode "username=admin" \
    --data-urlencode "password=Nexa@2026")
ADMIN_TOKEN=$(printf '%s' "$ADMIN_LOGIN" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.session.role!=="Admin"||j.session.token.length!==64)process.exit(1);
 process.stdout.write(j.session.token);
})')
echo "Admin login API: secure session issued"

curl -fsS --get "http://127.0.0.1:$PORT/api/admin/analytics" \
    --data-urlencode "token=$ADMIN_TOKEN" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.network.totalLocations!==15||j.fleet.totalRiders!==0||j.fleet.ratedRiders!==0||j.riderLeaderboard.length!==0)process.exit(1);
 console.log("Admin analytics API: empty fleet and network metrics confirmed");
})'

CUSTOM_CITY=$(curl -fsS -X POST "http://127.0.0.1:$PORT/api/locations/add" \
    --data-urlencode "token=$ADMIN_TOKEN" \
    --data-urlencode "locationName=Green Valley Depot" \
    --data-urlencode "locationType=Depot" \
    --data-urlencode "connectFrom=Central Courier Hub" \
    --data-urlencode "routeName=Green Valley Road" \
    --data-urlencode "distance=4.7" \
    --data-urlencode "baseTime=9" \
    --data-urlencode "traffic=Low")
printf '%s' "$CUSTOM_CITY" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);const location=j.locations.find(v=>v.name==="Green Valley Depot");
 const road=j.roads.find(v=>v.name==="Green Valley Road");
 if(!location||!road||j.customLocations!==1||j.customRoutes!==1)process.exit(1);
 console.log("Custom destination API: named location and connecting route added");
})'

curl -fsS --get "http://127.0.0.1:$PORT/api/route" \
    --data-urlencode "from=Central Courier Hub" \
    --data-urlencode "to=Green Valley Depot" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(!j.minimumDistance.found||j.minimumDistance.distance!==4.7)process.exit(1);
 console.log("Custom destination routing API: new location is reachable");
})'

curl -fsS -X POST "http://127.0.0.1:$PORT/api/roads/add" \
    --data-urlencode "token=$ADMIN_TOKEN" \
    --data-urlencode "routeName=Harbor Bypass" \
    --data-urlencode "from=11" \
    --data-urlencode "to=14" \
    --data-urlencode "distance=8.4" \
    --data-urlencode "baseTime=14" \
    --data-urlencode "traffic=High" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(!j.roads.some(r=>r.name==="Harbor Bypass"))process.exit(1);
 console.log("Named route API: existing locations connected");
})'

curl -fsS -X POST "http://127.0.0.1:$PORT/api/roads/block" \
    --data-urlencode "token=$ADMIN_TOKEN" \
    --data-urlencode "roadId=R22" \
    --data-urlencode "blocked=1" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(!j.roads.find(v=>v.id==="R22")?.blocked)process.exit(1);
 console.log("Protected road control API: road blocked");
})'
curl -fsS -X POST "http://127.0.0.1:$PORT/api/roads/undo" \
    --data-urlencode "token=$ADMIN_TOKEN" >/dev/null

NEW_RIDER=$(curl -fsS -X POST "http://127.0.0.1:$PORT/api/riders/register" \
    --data-urlencode "name=Areeba Khan" \
    --data-urlencode "phone=03005550109" \
    --data-urlencode "locationId=4" \
    --data-urlencode "vehicle=Bike" \
    --data-urlencode "registration=NXR-001" \
    --data-urlencode "baseCharge=140" \
    --data-urlencode "perKmCharge=31" \
    --data-urlencode "password=Secure@101")
NEW_RIDER_ID=$(printf '%s' "$NEW_RIDER" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.rider.id!=="RD-101"||j.session.role!=="Rider"||j.rider.locationId!==4||j.rider.rating!==null||j.rider.ratingCount!==0||j.rider.completed!==0||j.rider.earnings!==0)process.exit(1);
 process.stdout.write(j.rider.id);
})')
NEW_RIDER_TOKEN=$(printf '%s' "$NEW_RIDER" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>process.stdout.write(JSON.parse(x).session.token))')
echo "Rider registration API: clean profile and selected starting location confirmed"

RIDER_LOGIN=$(curl -fsS -X POST "http://127.0.0.1:$PORT/api/auth/rider/login" \
    --data-urlencode "riderId=$NEW_RIDER_ID" \
    --data-urlencode "password=Secure@101")
printf '%s' "$RIDER_LOGIN" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.session.role!=="Rider"||j.rider.id!=="RD-101"||!j.rider.phone)process.exit(1);
 console.log("Rider login API: registered private profile returned");
})'

curl -fsS --get "http://127.0.0.1:$PORT/api/rider/profile" \
    --data-urlencode "token=$NEW_RIDER_TOKEN" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.rider.id!=="RD-101"||j.rider.locationId!==4||!j.rider.registration)process.exit(1);
 console.log("Protected rider profile API: starting location restored");
})'

UPDATE_UNAUTHORIZED=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/api/riders/update" \
    --data-urlencode "riderId=$NEW_RIDER_ID" \
    --data-urlencode "available=1" \
    --data-urlencode "locationId=6" \
    --data-urlencode "baseCharge=155" \
    --data-urlencode "perKmCharge=34")
if [[ "$UPDATE_UNAUTHORIZED" != "401" ]]; then
    echo "Rider update endpoint was not protected."
    exit 1
fi
curl -fsS -X POST "http://127.0.0.1:$PORT/api/riders/update" \
    --data-urlencode "token=$NEW_RIDER_TOKEN" \
    --data-urlencode "riderId=$NEW_RIDER_ID" \
    --data-urlencode "available=1" \
    --data-urlencode "locationId=6" \
    --data-urlencode "baseCharge=155" \
    --data-urlencode "perKmCharge=34" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(!j.rider.available||j.rider.locationId!==6||j.rider.baseCharge!==155)process.exit(1);
 console.log("Rider profile API: protected location and pricing update confirmed");
})'

curl -fsS "http://127.0.0.1:$PORT/api/riders" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.riders.length!==1||j.riders[0].rating!==null||j.riders[0].ratingCount!==0||"phone" in j.riders[0]||"earnings" in j.riders[0])process.exit(1);
 console.log("Public rider API: dynamically registered profile is sanitized");
})'

QUICK_BOOKING=$(curl -fsS -X POST "http://127.0.0.1:$PORT/api/bookings" \
    --data-urlencode "customerName=Muhammad Saad" \
    --data-urlencode "receiverName=Hanzala Khan" \
    --data-urlencode "receiverPhone=03001234567" \
    --data-urlencode "pickupId=5" \
    --data-urlencode "destinationId=7" \
    --data-urlencode "weightKg=2.5" \
    --data-urlencode "priority=Express" \
    --data-urlencode "mode=quick")
QUICK_TRACKING=$(printf '%s' "$QUICK_BOOKING" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.status!=="Rider Assigned"||!j.rider)process.exit(1);
 process.stdout.write(j.trackingId);
})')
for _ in 1 2 3 4; do
    curl -fsS -X POST "http://127.0.0.1:$PORT/api/bookings/advance" \
        --data-urlencode "trackingId=$QUICK_TRACKING" >/dev/null
done
curl -fsS --get "http://127.0.0.1:$PORT/api/track" \
    --data-urlencode "id=$QUICK_TRACKING" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.status!=="Delivered")process.exit(1);
 console.log("Booking and tracking lifecycle API: delivered");
})'

curl -fsS -X POST "http://127.0.0.1:$PORT/api/bookings/rate" \
    --data-urlencode "trackingId=$QUICK_TRACKING" \
    --data-urlencode "rating=4" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.customerRating!==4||j.rider.rating!==4||j.rider.ratingCount!==1)process.exit(1);
 console.log("Customer rating API: completed delivery updates genuine rider rating");
})'

DUPLICATE_RATING=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/api/bookings/rate" \
    --data-urlencode "trackingId=$QUICK_TRACKING" \
    --data-urlencode "rating=5")
if [[ "$DUPLICATE_RATING" != "400" ]]; then
    echo "Duplicate customer rating was not rejected."
    exit 1
fi

curl -fsS --get "http://127.0.0.1:$PORT/api/admin/analytics" \
    --data-urlencode "token=$ADMIN_TOKEN" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.fleet.ratedRiders!==1||j.fleet.averageRating!==4)process.exit(1);
 console.log("Admin analytics API: submitted rider rating included dynamically");
})'

curl -fsS "http://127.0.0.1:$PORT/api/dashboard" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.totalBookings!==1||"totalRevenue" in j)process.exit(1);
 console.log("Public dashboard API: financial data hidden");
})'
curl -fsS --get "http://127.0.0.1:$PORT/api/admin/dashboard" \
    --data-urlencode "token=$ADMIN_TOKEN" | node -e '
let x="";process.stdin.on("data",d=>x+=d);process.stdin.on("end",()=>{
 const j=JSON.parse(x);if(j.deliveredBookings!==1||!(j.totalRevenue>0))process.exit(1);
 console.log("Admin dashboard API: protected financial data confirmed");
})'

curl -fsS "http://127.0.0.1:$PORT/" | grep -q "NexaRoute | Smart Route Optimizer"
curl -fsS "http://127.0.0.1:$PORT/style.css" | grep -q -- "--navy"
curl -fsS "http://127.0.0.1:$PORT/app.js" | grep -q "openNotifications"
if curl -fsS "http://127.0.0.1:$PORT/" | grep -Eqi "DSA|BFS|DFS|data structures|C\+\+ engine"; then
    echo "Product-facing implementation jargon is still present."
    exit 1
fi

echo "Frontend assets and product copy: OK"
echo "Smoke test passed."
