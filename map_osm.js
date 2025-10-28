let maxRecorded = "";
let map;
let streetLayer, satelliteLayer;

function clearMap() {
    if (map) {
        map.off();
        map.remove();
        map = null;
    }

    map = L.map('map', { zoomControl: true }).setView([baseLatitude, baseLongitude], 12);

    // Define all base layers
    const streetLayer = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; OpenStreetMap contributors'
    });

    const satelliteLayer = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {
        attribution: 'Tiles &copy; Esri &mdash; Source: Esri'
    });

    const googleStreetLayer = L.tileLayer('https://mt1.google.com/vt/lyrs=r&x={x}&y={y}&z={z}&key=' + googleApiKey, {
        attribution: '&copy; Google'
    });

    const googleSatelliteLayer = L.tileLayer('https://mt1.google.com/vt/lyrs=s&x={x}&y={y}&z={z}&key=' + googleApiKey, {
        attribution: '&copy; Google'
    });
    
    const esriTopo = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Topo_Map/MapServer/tile/{z}/{y}/{x}', {
        attribution: 'Tiles &copy; Esri &mdash; Source: Esri, HERE, Garmin, USGS, etc.'
    });
    
    
    const topoLayer = L.tileLayer('https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png', {
        maxZoom: 17,
        attribution: 'Map data: &copy; OpenStreetMap contributors, SRTM | Map style: &copy; OpenTopoMap (CC-BY-SA)'
    });

    const carto = L.tileLayer('https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png', {
        attribution: '&copy; OpenStreetMap contributors'
    });
    
    const rainViewer = L.tileLayer(
      'https://tilecache.rainviewer.com/v2/radar/{z}/{x}/{y}/256/2/1_1.png',
      {
        attribution: 'Weather radar © RainViewer',
        opacity: 0.6
      }
    );
    
   const nasaGibs = L.tileLayer(
    'https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/MODIS_Terra_CorrectedReflectance_TrueColor/default/{time}/{tilematrixset}{z}/{y}/{x}.jpg',
    {
      attribution: 'Imagery © NASA EOSDIS GIBS',
      tilematrixset: 'GoogleMapsCompatible_Level9',
      time: new Date().toISOString().split('T')[0], // today’s date in YYYY-MM-DD
      maxZoom: 9
    }
  );
  
  const cyclosm = L.tileLayer(
    'https://{s}.tile-cyclosm.openstreetmap.fr/cyclosm/{z}/{x}/{y}.png',
    {
      attribution: '&copy; OpenStreetMap contributors, CyclOSM'
    }
  );
  
  function tileXYToQuadKey(x, y, z) {
      let quadKey = '';
      for (let i = z; i > 0; i--) {
          let digit = 0;
          const mask = 1 << (i - 1);
          if ((x & mask) !== 0) digit += 1;
          if ((y & mask) !== 0) digit += 2;
          quadKey += digit;
      }
      return quadKey;
  }

  // Create a working Bing Aerial layer instance
  const bingLayer  = L.tileLayer('', {
      attribution: 'Bing Imagery &copy; Microsoft',
      subdomains: ['0','1','2','3','4'],
      minZoom: 1,
      maxZoom: 19,
      // Custom tile URL function
      tileSize: 256,
      zoomOffset: 0,
      detectRetina: false,
      getTileUrl: function(coords) {
          const q = tileXYToQuadKey(coords.x, coords.y, coords.z);
          return `https://ecn.t3.tiles.virtualearth.net/tiles/a${q}.jpeg?g=1`;
      }
  });

     // your OpenWeatherMap API key
    const apiKey = openWeatherApiKey;

    // choose one of their layer types:
    const clouds = L.tileLayer(
      `https://tile.openweathermap.org/map/clouds_new/{z}/{x}/{y}.png?appid=${apiKey}`,
      { opacity: 0.6, attribution: '&copy; <a href="https://openweathermap.org/">OpenWeatherMap</a>' }
    );

    const temp = L.tileLayer(
      `https://tile.openweathermap.org/map/temp_new/{z}/{x}/{y}.png?appid=${apiKey}`,
      { opacity: 0.5, attribution: '&copy; <a href="https://openweathermap.org/">OpenWeatherMap</a>' }
    );
    
    const precipitation = L.tileLayer(`https://tile.openweathermap.org/map/precipitation_new/{z}/{x}/{y}.png?appid=${apiKey}`, { opacity: 0.5, attribution: '&copy; OpenWeatherMap' });
    const wind = L.tileLayer(`https://tile.openweathermap.org/map/wind_new/{z}/{x}/{y}.png?appid=${apiKey}`, { opacity: 0.5, attribution: '&copy; OpenWeatherMap' });
    const pressure = L.tileLayer(`https://tile.openweathermap.org/map/pressure_new/{z}/{x}/{y}.png?appid=${apiKey}`, { opacity: 0.5, attribution: '&copy; OpenWeatherMap' });

  
    // Default layer
    streetLayer.addTo(map);

    // Make layer control
    baseLayers = {
        "OSM Street": streetLayer,
        "Carto": carto,
        "ESRI Satellite": satelliteLayer,
        "Google Street": googleStreetLayer,
        "Google Satellite": googleSatelliteLayer,
        "ESRI Topo": esriTopo,      
        "OpenTopoMap": topoLayer,   
        "RainViewer Radar": rainViewer,
        "NASA GIBS":nasaGibs,
        "Outdoors":cyclosm,
        "Bing Aerial": bingLayer,
   

    };
    
    overlays = {
         "Clouds": clouds,
        "Temperature": temp,
        "Pressure": pressure,
        "Wind": wind,
        "Precipitation": precipitation
    }

    //L.control.layers(baseLayers).addTo(map);
    L.control.layers(baseLayers, overlays).addTo(map);
    
    L.control.scale({
      position: 'topright',   // or 'topleft', 'bottomleft', 'bottomright'
      imperial: true,         // show miles/feet
      metric: true,           // show km/meters
      maxWidth: 200            // max length in pixels for the scale bar
    }).addTo(map);



    // assuming you already have a Leaflet map instance:
 

 
 

}

 

window.initMap =   function () {

    const queryParams = new URLSearchParams(window.location.search);
    let liveData = 0;
    let liveDataCheckbox = document.getElementById("live_data");
    if(liveDataCheckbox.checked) {
      liveData = 1;
    }
    if (!liveData || justLoaded) {
      clearMap();
    }
    //if liveData, not clear everything
    let locationIdArray = [];
    let scale = queryParams.get('scale');
    let locationId = queryParams.get('location_id');
    let periodAgo = queryParams.get('period_ago');
    let locationIds = queryParams.get('location_ids');
    let plotType = "single";
    let absoluteTimespanCusps = queryParams.get('absolute_timespan_cusps');
    let atcCheckbox = document.getElementById("atc_id");
    let allDataCheckbox = document.getElementById("all_data");
    let yearsAgoToShow = queryParams.get('years_ago');
    let absoluteTimeAgo = queryParams.get('absolute_time_ago');
    let allData = 0;
    let url = new URL(window.location.href);
    let colors = [];
 
    if(allDataCheckbox.checked) {
      allData = 1;
    }

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
    } 

    if(justLoaded){
      if(absoluteTimespanCusps == 1){
        atcCheckbox.checked = true;
      }
    } else {
      absoluteTimeAgo = "";
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
      if(!justLoaded){
        absoluteTimeAgo = currentStartDate; //hmm, we can do this instead now
        url.searchParams.set("absolute_time_ago", absoluteTimeAgo);
      }
    }	
    periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
    url.searchParams.set("period_ago", periodAgo);

    //update the URL without changing actual location
    history.pushState({}, "", url);
    let deviceDropdown = document.getElementById("locationDropdown");
    let deviceId = deviceDropdown[deviceDropdown.selectedIndex].value;
    // Create the map centered somewhere IN THE BESTEST COUNTRY IN THE UNIVERSE, MERKA!!!
    url = backendDataUrl("getMap", deviceId, allData, liveData, maxRecorded, scale, absoluteTimespanCusps, periodAgo, absoluteTimeAgo, 0);
  
    // XHR instead of fetch
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.onload = function () {
        if (xhr.status === 200) {
           const locations = JSON.parse(xhr.responseText);

            if (locations.points.length > 0) {
                const bounds = L.latLngBounds();
                const latestRecorded = Math.max(...locations.points.map(p => new Date(p.recorded)));
                locations.points.forEach((loc, i) => {
                    const latLng = [parseFloat(loc.latitude), parseFloat(loc.longitude)];
                    const deviceFound = findObjectByColumn(locations.devices, "device_id", loc.device_id);
                    let thumbnail = "";
                    if(deviceFound["thumbnail"]) {
                       thumbnail = buildUploadPath("device", loc.device_id, deviceFound["thumbnail"])
                     }
                    // Small circle marker


                    let marker;
                    const isLatest = new Date(loc.recorded).getTime() === latestRecorded;

                    if (isLatest && thumbnail) {
                        // Circular image marker for latest point
                        const html = `
                            <div style="
                                width: 40px;
                                height: 40px;
                                border-radius: 50%;
                                overflow: hidden;
                                border: 2px solid white;
                                box-shadow: 0 0 6px rgba(0,0,0,0.5);
                            ">
                                <img src='./${thumbnail}' style="width: 100%; height: 100%; object-fit: cover;">
                            </div>
                        `;

                        const icon = L.divIcon({
                            html,
                            className: '', // remove default leaflet styles
                            iconSize: [40, 40]
                        });

                        marker = L.marker(latLng, { icon }).addTo(map);
                    } else {
                        // Normal small circle for other points
                        marker = L.circle(latLng, {
                            radius: 10,
                            fillColor: deviceFound.color,
                            fillOpacity: 0.7,
                            stroke: false
                        }).addTo(map);
                    }



                    // Popup info
                    const content = `
                        <div style="font-size:12px; line-height:1.4;">
                            <!--<img src='./${thumbnail}' width=100><br/>-->
                            <strong>Recorded:</strong> ${loc.recorded}<br>
                            (${timeAgo(loc.recorded, null, false)})<br>
                            <strong>Speed:</strong> ${loc.wind_speed} m/s<br>
                            <strong>Elevation:</strong> ${loc.elevation} m<br>
                            <strong>Battery:</strong> ${loc.voltage}%
                        </div>
                    `;
                    marker.bindPopup(content);

                    // Polyline to next point
                    if (i < locations.points.length - 1) {
                        const nextLoc = locations.points[i + 1];
                        const nextLatLng = [parseFloat(nextLoc.latitude), parseFloat(nextLoc.longitude)];

                        const oldestDate = new Date(locations.points[0].recorded);
                        const newestDate = new Date(locations.points[locations.points.length - 1].recorded);
                        const recordedDate = new Date(loc.recorded);
                        const preAdjust = 0.8 * (1 - (recordedDate - oldestDate) / (newestDate - oldestDate));
                        const color = rainbowColor(preAdjust);

                        L.polyline([latLng, nextLatLng], {
                            color: color,
                            weight: 2,
                            opacity: 1
                        }).addTo(map);
                    }

                    bounds.extend(latLng);

                    if (!maxRecorded || loc.recorded > maxRecorded) {
                        maxRecorded = loc.recorded;
                    }
                });

                map.fitBounds(bounds);
                document.getElementById('greatestTime').innerHTML = " Latest Data: " + timeAgo(maxRecorded);
            }
            createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'initMap()', 'device_log', deviceId);
            if(liveData) {
              setTimeout(()=>{
              initMap();
              }, 3000);
            }
            justLoaded = false;

        } else {
            console.error('Error loading location data:', xhr.statusText);
        }
    };
    xhr.send();
};

function rainbowColor(value) {
    value = Math.min(1, Math.max(0, value));
    const hue = value * 360;
    return tinycolor({ h: hue, s: 1, l: 0.5 }).toHexString();
}

