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
      const response = await fetch("data.php?scale=" + scale + "&period_ago=" + periodAgo + "&mode=getMap&device_id=" + deviceId);
      const locations = await response.json();
          console.log(locations);
      // If the JSON looks like: [{latitude: 40.7, longitude: -74.0}, ...]
      locations.points.forEach(loc => {
        new google.maps.Marker({
          position: { lat: parseFloat(loc.latitude), lng: parseFloat(loc.longitude) },
          map: map
        });
      });

      // Optional: re-center map to fit all markers
      if (locations.points.length) {
        const bounds = new google.maps.LatLngBounds();
        locations.points.forEach(loc => bounds.extend({
          lat: parseFloat(loc.latitude), lng: parseFloat(loc.longitude)
        }));
        map.fitBounds(bounds);
      }
      //console.log(scaleConfig);
      createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'initMap()', 'device_log', deviceId);
      justLoaded = false;
    } catch (err) {
      console.error('Error loading location data:', err);
    }
  }