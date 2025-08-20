window.initMap = async function () {
    const queryParams = new URLSearchParams(window.location.search);
    let locationIdArray = [];
    let scale = queryParams.get('scale');
    let locationId = queryParams.get('location_id');
    let periodAgo = queryParams.get('period_ago');
    let locationIds = queryParams.get('location_ids');
    let plotType = "single";
    let absoluteTimespanCusps = queryParams.get('absolute_timespan_cusps');
    let atcCheckbox = document.getElementById("atc_id");
    let yearsAgoToShow = queryParams.get('years_ago');
    let greatestTime = "2000-01-01 00:00:00";

    let url = new URL(window.location.href);
    let colors = [];

    if(!scale){
      scale = "day";
    }
    let locationIdDropdown = document.getElementById('locationDropdown');
    if(!justLoaded){
      locationId  = locationIdDropdown[locationIdDropdown.selectedIndex].value
    }


    if(document.getElementById('scaleDropdown')  && !justLoaded){
      scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
    }	
    url.searchParams.set("scale", scale);

    if(atcCheckbox.checked) {
      absoluteTimespanCusps = 1;
    } else {
      absoluteTimespanCusps = 0;
    }
    if(justLoaded){
      if(absoluteTimespanCusps == 1){
        atcCheckbox.checked = true;
      }
    }
    url.searchParams.set("absolute_timespan_cusps", absoluteTimespanCusps);
	
    let periodAgoDropdown = document.getElementById('startDateDropdown');	

    if(periodAgoDropdown){
      if(!justLoaded){
        periodAgo = periodAgoDropdown[periodAgoDropdown.selectedIndex].value;
      } else {
        periodAgo = 0;
      }
      if(currentStartDate == periodAgoDropdown[periodAgoDropdown.selectedIndex].text){
        thisPeriod = periodAgo;
        periodAgo = false;
        //console.log("dates unchanged");
      }
      currentStartDate = periodAgoDropdown[periodAgoDropdown.selectedIndex].text;
    }	
    periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
    url.searchParams.set("period_ago", periodAgo);

    //update the URL without changing actual location
    history.pushState({}, "", url);
    let deviceDropdown = document.getElementById("locationDropdown");
    let deviceId = deviceDropdown[deviceDropdown.selectedIndex].value;
    // Create the map centered somewhere reasonable
    map = new google.maps.Map(document.getElementById('map'), {
      center: { lat: 40, lng: -100 }, // fallback center
      zoom: 4
    });

    try {
      // Fetch your JSON data from backend
      const response = await fetch("data.php?scale=" + scale + "&absolute_timespan_cusps=" + absoluteTimespanCusps + "&period_ago=" + periodAgo + "&mode=getMap&device_id=" + deviceId);
      const locations = await response.json();
          console.log(locations);
      // If the JSON looks like: [{latitude: 40.7, longitude: -74.0}, ...]
      /*
      locations.points.forEach(loc => {
        new google.maps.Marker({
          position: { lat: parseFloat(loc.latitude), lng: parseFloat(loc.longitude) },
          map: map
        });
      });
      */
      // Optional: re-center map to fit all markers
      if (locations.points.length > 1) {
        let oldestRecorded = locations.points.reduce((oldest, p) => {
          return p.recorded < oldest ? p.recorded : oldest;
        }, locations.points[0].recorded);
        let newestRecorded = locations.points.reduce((oldest, p) => {
          return p.recorded > oldest ? p.recorded : oldest;
        }, locations.points[0].recorded);
        for (let i = 0; i < locations.points.length; i++) {
          const loc = locations.points[i];
          const latLng = {
            lat: parseFloat(loc.latitude),
            lng: parseFloat(loc.longitude)
          };
 
          // draw small circle instead of marker
          new google.maps.Circle({
            strokeWeight: 0,
            fillColor: "#000",
            fillOpacity: 0.7,
            map,
            center: latLng,
            radius: 10 // in meters; make this tiny to look like a dot
          });

          // draw line segment to next point
          if (i < locations.points.length - 1) {
            const nextLoc = locations.points[i + 1];
            const nextLatLng = {
              lat: parseFloat(nextLoc.latitude),
              lng: parseFloat(nextLoc.longitude)
            };

            // age calculation
            const oldestDate = new Date(oldestRecorded);
            const recordedDate = new Date(loc.recorded);
            const newestDate = new Date(newestRecorded);
            let ageMinutes = (recordedDate - oldestDate) /(1000 * 60 * 60);
            //console.log(oldestRecorded, loc.recorded, ageMinutes);
            //let color = "#00ff00";
            //if (ageMinutes > 5 && ageMinutes <= 20) {
             // color = "#ffaa00";
            //} else if (ageMinutes > 20) {
              //color = "#ff0000";
            //}
            //let colors = ["#ff0000", "#00ff00", "#0000ff", "#ff00ff", "#ffaa00"];
            //color = colors[parseInt(Math.random() * colors.length)];
            //console.log(color, ageMinutes);
            const preAdjust =  (recordedDate-oldestDate)/(newestDate-oldestDate);
            const hue = (preAdjust * 360);
            //const adjustedValue = rainbowColor(Math.pow(hue, 0.7)); // tweak exponent to shift distribution
            const color = rainbowColor(preAdjust); // tweak exponent to shift distribution

            colors[colors.length] = color;
            console.log(colors[colors.length-1]);
  
            let polyline = new google.maps.Polyline({
              path: [latLng, nextLatLng],
              geodesic: true,
              strokeColor: colors[colors.length - 1],
              strokeOpacity: 1.0,
              strokeWeight: 2 
            });
            polyline.setMap(map);
          }
        }
        // Optional: re-center map to fit all markers
        if (locations.points.length) {
          const bounds = new google.maps.LatLngBounds();
          locations.points.forEach(loc => bounds.extend({
            lat: parseFloat(loc.latitude), lng: parseFloat(loc.longitude)
          }));
          map.fitBounds(bounds);
        }
      }
      //console.log(scaleConfig);
      createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'initMap()', 'device_log', deviceId);
      justLoaded = false;
    } catch (err) {
      console.error('Error loading location data:', err);
    }
  }
  
  function rainbowColor(value) {
    // clamp between 0 and 1
    value = Math.min(1, Math.max(0, value));

    // map value [0,1] -> hue [0,360)
    const hue = value * 360;

    // full saturation and brightness
    return tinycolor({ h: hue, s: 1, l: 0.5 }).toHexString();
}