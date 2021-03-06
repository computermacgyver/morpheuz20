/* 
 * Morpheuz Sleep Monitor
 *
 * Copyright (c) 2013-2015 James Fowler
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*global mConst, clearTimeout, nvl, window, btoa, FormData */
/*exported makePostAjaxCall, makeGetAjaxCall, turnLifxLightsOn, makeAjaxCall */

/*
 * Standard Post Ajax call routine
 */
function makePostAjaxCall(url, msg, resp) {
  var tout = setTimeout(function() {
    resp({
      "status" : 0,
      "errors" : [ "timeout" ]
    });
  }, mConst().timeout);
  var req = new XMLHttpRequest();
  req.open("POST", url, true);
  req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  req.setRequestHeader("Content-length", msg.length);
  req.setRequestHeader("Connection", "close");
  req.timeout = mConst().timeout;
  req.ontimeout = function() {
    resp({
      "status" : 0,
      "errors" : [ "timeout" ]
    });
    clearTimeout(tout);
  };
  req.onload = function() {
    if (req.readyState === 4 && req.status === 200) {
      clearTimeout(tout);
      var result = JSON.parse(req.responseText);
      resp(result);
    } else if (req.readyState === 4 && (req.status >= 400 && req.status < 500)) {
      clearTimeout(tout);
      var result2 = JSON.parse(req.responseText);
      resp(result2);
    } else if (req.readyState === 4 && (req.status === 500)) {
      clearTimeout(tout);
      resp({
        "status" : 0,
        "errors" : [ "500 error" ]
      });
    }
  };
  req.send(msg);
}

/*
 * Standard Get Ajax call routine
 */
function makeGetAjaxCall(url, resp) {
  var tout = setTimeout(function() {
    resp({
      "status" : 0,
      "errors" : [ "timeout" ]
    });
  }, mConst().timeout);
  var req = new XMLHttpRequest();
  req.open("GET", url, true);
  req.setRequestHeader("Connection", "close");
  req.timeout = mConst().timeout;
  req.ontimeout = function() {
    resp({
      "status" : 0,
      "errors" : [ "timeout" ]
    });
    clearTimeout(tout);
  };
  req.onload = function() {
    if (req.readyState === 4 && req.status === 200) {
      clearTimeout(tout);
      resp({
        "status" : 1,
        "data" : req.responseText
      });
    } else if (req.readyState === 4 && (req.status >= 300 && req.status <= 599)) {
      clearTimeout(tout);
      resp({
        "status" : 0,
        "errors" : [ req.status, nvl(req.responseText, "No Msg") ]
      });
    }
  };
  req.send();

}

/*
 * If LIFX values are set, this function will turn on all the lights with a custom fade-in time
 */
function turnLifxLightsOn() {
  
  // Find out config information
  var token = nvl(window.localStorage.getItem("lifx-token"), "");
  var fadeInTime = nvl(window.localStorage.getItem("lifx-time"), mConst().lifxTimeDef);
  
  // Escape if not configured
  if (token === "" || fadeInTime === "") {
    console.log("lifx control deactivated");
    return;
  }

  var url = "https://api.lifx.com/v1beta1/lights/all/power";
  var method = "PUT";
  var data = new FormData();

  data.append('state', 'on');
  data.append('duration', fadeInTime);

  var req = new XMLHttpRequest();
  req.open(method, url, true);
  req.setRequestHeader("Authorization", "Basic " + btoa(token + ":" + ""));
  req.withCredentials = "true";
  req.send(data);

  req.onload = function() {
    //response
  };

}

/*
 * Generic ajax support function (used by Maker and Hue bulb interface)
 */
function makeAjaxCall(mode, url, toTime, dataout, resp) {
  var tout = setTimeout(function() {
    resp({
      "status" : 0,
      "errors" : [ "timeout" ]
    });
  }, toTime);
  var req = new XMLHttpRequest();
  req.open(mode, url, true);
  req.setRequestHeader("Content-Type", "application/json");
  req.timeout = toTime;
  req.ontimeout = function() {
    resp({
      "status" : 0,
      "errors" : [ "timeout" ]
    });
    clearTimeout(tout);
  };
  req.onload = function() {
    if (req.readyState === 4 && req.status === 200) {
      clearTimeout(tout);
      resp({
        "status" : 1,
        "data" : safeJSONparse(req.responseText)
      });
    } else if (req.readyState === 4 && (req.status >= 300 && req.status <= 599)) {
      clearTimeout(tout);
      resp({
        "status" : 0,
        "errors" : [ req.status, nvl(req.responseText, "No Msg") ]
      });
    }
  };
  if (dataout === "") {
    req.send();
  } else {
	req.send(dataout);
  }

}

/*
 * JSON parse with built in safety - like if the message isn't bloody json
 */
function safeJSONparse(txt) {
  try {
    return JSON.parse(txt);
  } catch (err) {
    return txt;
  }
}