let glblChart = null;
let graphDataObject = {};
let columnsWeCareAbout = ["solar_power","load_power","battery_power","battery_percentage", "weather_condition_id", "digest"]; //these are the inverter columns to be graphed from inverter_log. if you have more, you can include them
let yearsIntoThePastWeCareAbout = [0,1,2,3];
//For graphs info, visit: https://www.chartjs.org
let timeStamp = [];
let weatherColorMap = [];
let segmentRects = [];
const bitColorMap = {};
let deviceFeatures = [];
let weatherConditions = [];

resetGraphData();

function resetGraphData(){
	graphDataObject = {};
	pastYearsViewed = [];
	for (let year of yearsIntoThePastWeCareAbout) { 
		for (let column of columnsWeCareAbout) {
			if (!graphDataObject[year]) {
				graphDataObject[year] = {};
			}
			if (!graphDataObject[year][column]) {
				graphDataObject[year][column] = [];
			}
		}
	}
}

function hexToRgba(hex, alpha = 0.4) {
  // Remove leading '#' if present
  hex = hex.replace(/^#/, '');

  // Handle shorthand hex colors like 'f80' -> 'ff8800'
  if (hex.length === 3) {
    hex = hex.split('').map(char => char + char).join('');
  }
  const bigint = parseInt(hex, 16);
  const r = (bigint >> 16) & 255;
  const g = (bigint >> 8) & 255;
  const b = bigint & 255;
  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}

function stringToColor(str) {
  let hash = 0;
  if(str){
    for (let i = 0; i < str.length; i++) {
      // Simple hash function: shift and sum character codes
      hash = str.charCodeAt(i) + ((hash << 5) - hash);
      hash = hash & hash; // Convert to 32bit integer
    }
  }   
  
  // Generate HSL values
  const hue = Math.abs(hash) % 360; // Hue between 0 and 359
  const saturation = 70; // Fixed saturation
  const lightness = 60;  // Fixed lightness
  const alpha = 0.3; // Adjust this value for desired transparency
  return `hsl(${hue}, ${saturation}%, ${lightness}%, ${alpha})`;
}

const weatherPlugin = {
  id: 'weatherSegments',
  beforeDatasetsDraw(chart, args, options) {
    if (!options.enabled) return;
    const { ctx, chartArea: { bottom } } = chart;
    const xScale = chart.scales['x']; // Access the x-axis scale by its ID
    const segmentHeight = 5;
    const segments = options.segments || [];

    if (!xScale) {
      console.warn('X-axis scale not found');
      return;
    }

    segments.forEach(segment => {
		if (!weatherColorMap[segment.weather]) {
			weatherColorMap[segment.weather] = stringToColor(segment.weather);

		}
    let fillColor = weatherColorMap[segment.weather];
    let label = "";
    const weatherRecordFound = findObjectByColumn(weatherConditions, "weather_condition_id", segment.weather);
    
    //console.log(segment.weather_condition_id, weatherRecordFound);
    if(weatherRecordFound) {
      fillColor = weatherRecordFound["color"];
      label = weatherRecordFound["name"];
    }
		const xStart = xScale.getPixelForValue(new Date(segment.start).getTime());
		const xEnd = xScale.getPixelForValue(new Date(segment.end).getTime());
		const width = xEnd - xStart;


		ctx.fillStyle = fillColor;
		ctx.fillRect(xStart, bottom, width, segmentHeight);

 
		// Store segment boundaries for interaction
		segmentRects.push({
			x: xStart,
			y: bottom,
			width: width,
			height: segmentHeight,
			weather: label
		});

    });
  }
};


const digestPlugin = {
  id: 'digestSegments',
  beforeDatasetsDraw(chart, args, options) {
    if (!options.enabled) return;
    const { ctx, chartArea, scales: { x } } = chart;
    const segments = options.segments || [];
    const totalBits = 32;
    const segmentHeight = 4;
    const spacing = 0;
    const topY = chartArea.bottom - 62; //negative is higher
    let rowIndex = 0;
    const sortedSegments = segments.slice().sort((a, b) =>
      new Date(a.start) - new Date(b.start)
    );
 
    const bitStates = Array.from({ length: totalBits }, () => ({
      active: false,
      currentStart: null,
      ranges: []
    }));

    sortedSegments.forEach(segment => {
      const start = new Date(segment.start);
      const end = new Date(segment.end);
      const digest = segment.digest;

      for (let bit = 0; bit < totalBits; bit++) {
        const state = bitStates[bit];
        const bitOn = digest != null && ((digest >> bit) & 1) === 1;

        if (bitOn) {
          if (!state.active) {
            state.currentStart = start;
            state.active = true;
          }
        } else {
          if (state.active) {
            state.ranges.push([state.currentStart, start]);
            state.currentStart = null;
            state.active = false;
          }
        }

        // Store the last seen end time for later flushing
        state.lastSeenEnd = end;
      }
    });

    // Flush only bits still active at the end
    bitStates.forEach(state => {
      if (state.active) {
        state.ranges.push([state.currentStart, state.lastSeenEnd]);
        state.currentStart = null;
        state.active = false;
      }
    });
    
    const visibleMin = x.min;
    const visibleMax = x.max;
    const visibleBits = [];
    bitStates.forEach((state, bit) => {
      //bitStates.forEach((state, bit) => {
      //const y = topY + bit * (segmentHeight + spacing);
      //this part does not seem to work:
      const hasVisibleSegment = state.ranges.some(([start, end]) => {
        const startTime = start.getTime();
        const endTime = end.getTime();
        return endTime >= visibleMin && startTime <= visibleMax;
      });
      if (hasVisibleSegment) {
        visibleBits.push({ bit, state });
      }
    });
    visibleBits.forEach(({ bit, state }, rowIndex) => {
      const y = topY + rowIndex * (segmentHeight + spacing);

      let label = "Unknown device feature";
      let hexColor = `hsl(${bit * 47 % 360}, 70%, 70%)`;

      const recordFound = findObjectByColumn(deviceFeatures, "digest_bit_position", String(bit));
      if (recordFound) {
        hexColor = recordFound.color || "#cccccc";
        label = recordFound.name || label;
      }

      ctx.fillStyle = hexToRgba(hexColor, 0.7);

      state.ranges.forEach(([start, end]) => {
        const xStart = x.getPixelForValue(start);
        const xEnd = x.getPixelForValue(end);
        const width = xEnd - xStart;

        if (width > 0) {
          ctx.fillRect(xStart, y, width, segmentHeight);
           segmentRects.push({
            x: xStart,
            y: y,
            width: width,
            height: segmentHeight,
            weather: label
          });
        }
      });
    });


  }
};

function showGraph(yearsAgo){
	let colorSeries = ["#ff0000", "#00ff00", "#0000ff", "#009999", "#3300ff", "#ff0033", "#ff3300", "33ff00", "#0033ff", "#6600cc", "#ff0066", "#cc6600", "66cc00", "#0066cc"];
	//console.log(timeStamp);
	if(glblChart){
		glblChart.destroy();
	}
	if(!yearsAgo){
		yearsAgo = 0;
	}
    let ctx = document.getElementById("Chart").getContext('2d');
	let columnCount = 0;
	let chartDataSet = [];
	let noCaptionsColumns = ["weather_condition_id", "digest"];
	for (let column of columnsWeCareAbout){
		if(noCaptionsColumns.indexOf(column) > -1) {
			continue;
		}
		let yAxisId = "A";
		if(column == "battery_percentage"){ //if you have a percentage instead of a kilowatt value, this is the scale you want
			yAxisId = "B";
		}
		//console.log(graphDataObject[0][column]);
		let legend = {
            label: column,
            fill: false,  //Try with true
            backgroundColor: colorSeries[columnCount],
            borderColor: colorSeries[columnCount],
            data: graphDataObject[0][column],
            yAxisID: yAxisId,
            borderWidth: 1
        };

		chartDataSet.push(legend);
		columnCount++;
	}

	const energyChart = new Chart(ctx, {
		type: 'line',
		data: {
			labels: timeStamp,
			datasets: chartDataSet
		},
		options: {
			responsive: true,
			maintainAspectRatio: false,
			interaction: {
        mode: 'nearest',
        intersect: false  // allows tooltips even if cursor isn't directly over a point
      },
			plugins: {
				weatherSegments: {
					segments:  [ ],
					enabled: true
				},
				digestSegments:{
					segments: [],
					enabled: true
				}
			},
			title: {
				display: true,
				text: 'Inverter data'
			}
			,
			elements: {
			point: {
				radius: 0,
				pointHoverRadius: 3,
				hitRadius: 10
			},
			line: {
				 tension: 0.2 // Smoothening (Curved) of data lines
			}
			},
			scales: {
			x: {
				type: 'time',
				title: {
				display: true,
				text: 'Time'
				}
			},
			A: {
				type: 'linear',
				position: 'left',
				title: {
				display: true,
				text: 'Power (Watts)'
				}
			},
			B: {
				type: 'linear',
				position: 'right',
				title: {
				display: true,
				text: 'Percentage'
				},
				grid: {
				drawOnChartArea: false
				}
			}
			}
		},
		plugins: [weatherPlugin, digestPlugin]
	});
 
	return energyChart;
}

const canvas = document.getElementById("Chart");


canvas.addEventListener("mousemove", (event) => {
  const rect = canvas.getBoundingClientRect();
  const mouseX = event.clientX - rect.left;
  const mouseY = event.clientY - rect.top;

  // Check if mouse is over any segment
  const hoveredSegment = segmentRects.find(seg =>
    mouseX >= seg.x &&
    mouseX <= seg.x + seg.width &&
    mouseY >= seg.y &&
    mouseY <= seg.y + seg.height
  );

  if (hoveredSegment) {
    // Display tooltip or perform desired action
    showTooltip(event.clientX, event.clientY, hoveredSegment.weather);
  } else {
    hideTooltip();
  }
});

const tooltip = document.createElement("div");
  tooltip.style.position = "absolute";
  tooltip.style.background = "rgba(0, 0, 0, 0.7)";
  tooltip.style.color = "#fff";
  tooltip.style.padding = "5px 10px";
  tooltip.style.borderRadius = "4px";
  tooltip.style.pointerEvents = "none";
  tooltip.style.transition = "opacity 0.2s";
  tooltip.style.opacity = 0;
  document.body.appendChild(tooltip);

function showTooltip(x, y, text) {
  tooltip.textContent = text;
  tooltip.style.left = `${x + 10}px`;
  tooltip.style.top = `${y + 10}px`;
  tooltip.style.opacity = 1;
  if(glblChart) {
    glblChart.options.plugins.tooltip.enabled = false;
    glblChart.update();
  }
}

function hideTooltip() {
  if(glblChart) {
    glblChart.options.plugins.tooltip.enabled = true;
    glblChart.update();
  }
  tooltip.style.opacity = 0;
}

//On Page load show graphs
window.onload = function() {
	console.log(new Date().toLocaleTimeString());
	Chart.register(weatherPlugin);
	Chart.register(digestPlugin);
	
};

let currentStartDate; //a global that needs to persist through HTTP sessions in the frontend
let justLoaded = true;

function getInverterData(yearsAgo) {
	const queryParams = new URLSearchParams(window.location.search);
	let scale = queryParams.get('scale');
	let absoluteTimespanCusps = queryParams.get('absolute_timespan_cusps');
	let atcCheckbox = document.getElementById("atc_id");
	let url = new URL(window.location.href);
	let greatestTime = "2000-01-01 00:00:00";
  let absoluteTimeAgo = queryParams.get('absolute_time_ago');
  
	if(justLoaded){
		if(absoluteTimespanCusps == 1){
			atcCheckbox.checked = true;
		}
	} else {
    absoluteTimeAgo = "";
	}
	if(atcCheckbox.checked) {
		absoluteTimespanCusps = 1;
	} else {
		absoluteTimespanCusps = 0;
	}
	url.searchParams.set("absolute_timespan_cusps", absoluteTimespanCusps);
	if(!scale){
		scale = "day";
	}
	let periodAgo = queryParams.get('period_ago');
 
	//console.log("got data");

	if(document.getElementById('scaleDropdown') && !justLoaded){
		scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
	}
	url.searchParams.set("scale", scale);
	//make the startDateDropdown switch to the appropriate item on the new scale:
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
		}
		currentStartDate = periodAgoDropdown[periodAgoDropdown.selectedIndex].text;
    if(!justLoaded){
      absoluteTimeAgo = currentStartDate; //hmm, we can do this instead now
      url.searchParams.set("absolute_time_ago", absoluteTimeAgo);
    }
	}	
	periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
	url.searchParams.set("period_ago", periodAgo);
	resetGraphData();
	segmentRects = [];
	history.pushState({}, "", url);
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "./data.php?scale=" + scale + "&period_ago=" + periodAgo + "&mode=getInverterData&absolute_timespan_cusps=" + absoluteTimespanCusps;
	if(absoluteTimeAgo) {
    endpointUrl = "./data.php?scale=" + scale + "&absolute_time_ago=" + absoluteTimeAgo + "&mode=getInverterData&absolute_timespan_cusps=" + absoluteTimespanCusps;
	}
	console.log(endpointUrl);

	xhttp.onreadystatechange = function() {
		let weatherSegmentsLocal = [];
		let digestSegmentsLocal = [];
		let datumCount = 0;
		let lastWeather = "";
		let lastWeatherTime = "";
		let lastDigest = "";
		let lastDigestTime = "";
	    if (this.readyState == 4 && this.status == 200) {
        timeStamp = [];
        //let time = new Date().toLocaleTimeString();
        let dataObject = JSON.parse(this.responseText); 
         console.log(dataObject);
        if(dataObject) {
          if(dataObject["sql"] && dataObject["error"]){
            console.log(dataObject["sql"], dataObject["error"]);
          } else {
            if(dataObject["device_features"]) { //we need this to label device feature ons and offs on the graph
              deviceFeatures = dataObject["device_features"]; //set the global deviceFeatures
            }
            if(dataObject["weather_conditions"]) { //we need this to label device feature ons and offs on the graph
              weatherConditions = dataObject["weather_conditions"]; //set the global deviceFeatures
            }
            if(dataObject["inverter_data"]) {
              for(let datum of dataObject["inverter_data"]) {
                datumCount++;
                //also show: weatherSegments 
                let time = datum["recorded"];
                for (let column of columnsWeCareAbout){
                    let value = datum[column];
                    if (column == "weather_condition_id") {
                      if(lastWeather != value) {
                        if(lastWeather == "") { //we're at the very beginning
                        } else {
                          weatherSegmentsLocal.push({"start": lastWeatherTime, "end": time, "weather": lastWeather});
                        }
                        lastWeather = value;
                        lastWeatherTime = time;
  
                      }
                      if (datumCount == dataObject["inverter_data"].length) { //get the last one too
                        weatherSegmentsLocal.push({"start": lastWeatherTime, "end": time, "weather": lastWeather});
                      }
                      
                    } else if (column == "digest") {
                    
                      if(lastDigest != value) {
                        if(lastDigest == "") { //we're at the very beginning
                        } else {
                          digestSegmentsLocal.push({"start": lastDigestTime, "end": time, "digest": parseInt(lastDigest)});
                        }
                        lastDigest = value;
                        lastDigestTime = time;
                      }
                      if (datumCount == dataObject["inverter_data"].length) { //get the last one too
                        digestSegmentsLocal.push({"start": lastDigestTime, "end": time, "digest": parseInt(lastDigest)});
                      }
                      
                    } else {
                      graphDataObject[yearsAgo][column].push(parseInt(value)); //parseInt is important because smoothArray was thinking the values might be strings
                    }
                    
                      
                  }
                  timeStamp.push(time);
                  if(time> greatestTime) {
                    greatestTime = time; //greatestTime is a global
                  }
                }
                
              }
            }
        } else {
          console.log("No data was found.");
        }
   
        if(scale == "hour"  || scale == "three-hour"  || scale == "day"){
          let batteryPercents =  graphDataObject[yearsAgo]["battery_percentage"];
          graphDataObject[yearsAgo]["battery_percentage"] = smoothArray(batteryPercents, 19, 1);
        }
        //console.log(weatherSegmentsLocal);
        console.log(digestSegmentsLocal);
        glblChart = showGraph();  //Update Graphs
        glblChart.options.plugins.weatherSegments.segments = weatherSegmentsLocal;
        glblChart.options.plugins.digestSegments.segments = digestSegmentsLocal;
        glblChart.update();
        document.getElementById('greatestTime').innerHTML = " Latest Data: " + timeAgo(greatestTime);
	    }
		document.getElementsByClassName("outercontent")[0].style.backgroundColor='#ffffff';
		justLoaded = false;
	};
	  

  xhttp.open("GET", endpointUrl, true);  
  xhttp.send();
  createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'getInverterData(0)', 'inverter_log', '');
}
 
getInverterData(0);
 

 