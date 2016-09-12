function makeButtons(suffix) {
    document.write('<div class="ui-grid-a">');
    document.write('<div class="ui-block-a"><input id="cancel_' + suffix + '" value="' + __Cancel + '" type="submit"></div>')
    document.write('<div class="ui-block-b"><input id="save_' + suffix + '" data-theme="a" value="' + __Save + '" type="submit"></div>')
    document.write('</div>')
}

function storeIntResult(options, keyword) {
    var value = $("#" + keyword).val();
    value = parseInt(value, 10);
    if (!isNaN(value)) {
	options[keyword] = value;
    }
}
function storeStringResult(options, keyword) {
    var value = $("#" + keyword).val();
    if (value) {
	options[keyword] = value;
    }
}

var storeResults = [];

function makeOption(keyword, label, options, storeResult) {
    var role = "slider";
    if (options) {
	role = "select";
    } else {
	options = [[0, __No], [1, __Yes]];
    }
    if (!storeResult) {
	storeResult = storeIntResult;
    }
    storeResults.push([keyword, storeResult]);

    document.write('<div data-role="fieldcontain"><label for="' + keyword + '">' + label + '</label><select name="' + keyword + '" id="' + keyword + '" data-role="' + role + '">');
    var key = $.url().param(keyword);
    for (var oi in options) {
	if (key == options[oi][0]) {
	    document.write('<option value="' + options[oi][0] + '" selected>' + options[oi][1] + '</option>');
	} else {
	    document.write('<option value="' + options[oi][0] + '">' + options[oi][1] + '</option>');
	}
    }
    document.write('</select></div>');
};

makeButtons('a');

makeOption("second_hand", __SecondHand);

if ($.url().param("sweep_seconds") != undefined) {
    makeOption("sweep_seconds", __SweepSeconds);
}

var num_faces = $.url().param("num_faces");
if (num_faces > 1) {
    var faces = [
	[ 0, __One ],
	[ 1, __Two ],
	[ 2, __Three ],
	[ 3, __Four ],
	[ 4, __Five ],
	[ 5, __Six ],
	[ 6, __Seven ],
	[ 7, __Eight ],
	[ 8, __Nine ],
	[ 9, __Ten ],
    ];
    makeOption("face_index", __FaceIndex, faces.slice(0, num_faces));
}

var num_face_colors = $.url().param("num_face_colors");
if (num_face_colors > 1) {
    var face_colors = [
	[ 0, __One ],
	[ 1, __Two ],
	[ 2, __Three ],
	[ 3, __Four ],
	[ 4, __Five ],
	[ 5, __Six ],
	[ 6, __Seven ],
	[ 7, __Eight ],
	[ 8, __Nine ],
	[ 9, __Ten ],
    ];
    makeOption("color_mode", __ColorMode, face_colors.slice(0, num_face_colors));
}

makeOption("draw_mode", __DrawMode);

if ($.url().param("chrono_dial") != undefined) {
    makeOption("chrono_dial", __ChronoDial,
	       [[0, __Off], [1, __ChronoTenths], [2, __ChronoHours], [3, __ChronoDual]]);
}

// This list is duplicated in resources/make_lang.py.
var langs = [
    [ 'en_US', 'English', 'latin', 0 ],
    [ 'fr_FR', 'French', 'latin', 1 ],
    [ 'it_IT', 'Italian', 'latin', 2 ],
    [ 'es_ES', 'Spanish', 'latin', 3 ],
    [ 'pt_PT', 'Portuguese', 'latin', 4 ],
    [ 'de_DE', 'German', 'latin', 5 ],
    [ 'nl_NL', 'Dutch', 'latin', 6 ],
    [ 'da_DK', 'Danish', 'latin', 7 ],
    [ 'sv_SE', 'Swedish', 'latin', 8 ],
    [ 'is_IS', 'Icelandic', 'latin', 9 ],
    [ 'tl', 'Tagalog', 'latin', 10 ],
    [ 'el_GR', 'Greek', 'extended', 11 ],
    [ 'hu_HU', 'Hungarian', 'latin', 12 ],
    [ 'ru_RU', 'Russian', 'extended', 13 ],
    [ 'pl_PL', 'Polish', 'latin', 14 ],
    [ 'cs_CZ', 'Czech', 'latin', 15 ],
    [ 'hy_AM', 'Armenian', 'extended', 16 ],
    [ 'tr_TR', 'Turkish', 'latin', 17 ],
    [ 'he_IL', 'Hebrew', 'rtl', 18 ],
    [ 'fa_IR', 'Farsi', 'rtl', 19 ],
    [ 'ar_SA', 'Arabic', 'rtl', 20 ],
    [ 'zh_CN', 'Chinese', 'zh', 21 ],
    [ 'ja_JP', 'Japanese', 'ja', 22 ],
    [ 'ko_KR', 'Korean', 'ko', 23 ],
    [ 'th_TH', 'Thai', 'th', 24 ],
    [ 'ta_IN', 'Tamil', 'ta', 25 ],
    [ 'hi_IN', 'Hindi', 'hi', 26 ],
    [ 'bg_BG', 'Bulgarian', 'extended', 27 ],
    [ 'nb_NO', 'Norwegian', 'latin', 28 ],
];

function sortComparer(a, b){
    return a[1].localeCompare(b[1])
};

var lang_options = [];
for (var oi in langs) {
    var lang_id = langs[oi][0];
    var key = '__lang_' + lang_id;
    var name = window[key];
    if (!name) {
	name = key;
    }
    lang_options.push([lang_id, name]);
}
lang_options.sort(sortComparer);


var date_window_options = [
    [0, __Off],
    [1, __DateIdentifyWindow],
    [2, __DateNumericDate],
    [6, __DateWeekday],
    [7, __DateMonth],
    [3, __DateYear],
    [5, __DateWeekNumber],
    [8, __DateAmPm],
    [4, __DateOrdinal],
    [10, __DateStepCount],
    [11, __DateStepCount10],
    [12, __DateActiveTime],
    [13, __DateWalkedDistance],
    [14, __DateSleepTime],
    [15, __DateSleepRestfulTime],
    [16, __DateCaloriesBurned],
    [17, __DateHeartRate],
];

if ($.url().param("show_debug")) {
    date_window_options.push([201, "(dev) heap free"]);
    date_window_options.push([202, "(dev) memory panic count"]);
    date_window_options.push([203, "(dev) resource reads"]);
    date_window_options.push([204, "(dev) draw face count"]);
}

var top_subdial_options = [
    [0, __Off],
    [1, __PebbleLabel],
    [2, __LunarPhase],
];

if ($.url().param("top_subdial")) {
    makeOption("top_subdial", __TopSubdial, top_subdial_options);
}

var date_window_keys = $.url().param("date_window_keys");
if (date_window_keys) {
  var num_date_windows = date_window_keys.length;
  if (num_date_windows) {
    for (var i = 0; i < num_date_windows; ++i) {
        var key = date_window_keys.charAt(i);
	var sym = 'date_window_' + key;
	var label = __DateWindowNamePrefix + key.toUpperCase() + __DateWindowNameSuffix;
	makeOption(sym, label, date_window_options, storeIntResult);
    }

    makeOption("display_lang", __DisplayLang, lang_options, storeStringResult);
  }
}

var week_numbering_options = [
    [0, __WeekMon4],
    [1, __WeekSun1],
    [2, __WeekSat1],
];

if ($.url().param("week_numbering")) {
    makeOption("week_numbering", __WeekNumbering, week_numbering_options);
}

makeOption("lunar_background", __LunarBackground,
	   [[0, __LunarBackgroundMatch], [1, __LunarBackgroundBlack]]);
makeOption("lunar_direction", __LunarDirection,
	   [[0, __LunarDirectionNorth], [1, __LunarDirectionSouth]]);

makeOption("hour_buzzer", __HourBuzzer);
makeOption("bluetooth_buzzer", __BluetoothBuzzer);
makeOption("bluetooth_indicator", __BluetoothIndicator,
	   [[0, __Off], [1, __WhenNeeded], [2, __Always]]);
makeOption("battery_gauge", __BatteryGauge,
	   [[0, __Off], [1, __WhenNeeded], [2, __Always], [3, __DigitalBattery]]);

if ($.url().param("show_debug")) {
    makeOption("show_debug", "Show developer options");
}

function saveOptions() {
    var options = {
    };

    for (var ri in storeResults) {
	var keyword = storeResults[ri][0];
	var storeResult = storeResults[ri][1];
	storeResult(options, keyword);
    }

    // Check for the developer Easter egg: set first date window to
    // Identify, second date window to am/pm, language to Tamil,
    // invert face on.  This reveals the option to enable developer
    // options.
    if (date_window_keys) {
        var dw0 = 'date_window_' + date_window_keys.charAt(0);
        var dw1 = 'date_window_' + date_window_keys.charAt(1);
        if (options[dw0] == 1 &&
            options[dw1] == 8 &&
            options['display_lang'] == 'ta_IN' &&
            options['draw_mode'] == 1) {
            options['show_debug'] = 1;
        }
    }

    return options;
}

makeButtons('b');
document.getElementById('loading_header').innerHTML = '<h3>' + $.url().param("watch_name") + '</h3>'

document.write('<div data-theme="a" data-role="footer">' + $.url().param("watch_name") + ' version ' + $.url().param("version") + '</div>')
