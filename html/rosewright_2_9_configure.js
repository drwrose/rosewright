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
	options = [[0, 'Off'], [1, 'On']];
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

makeOption("second_hand", "Second hand");

if ($.url().param("sweep_seconds") != undefined) {
    makeOption("sweep_seconds", "Smooth-sweep seconds (heavy battery use)");
}	

var num_faces = $.url().param("num_faces");
if (num_faces > 1) {
    var faces = [
	[ 0, 'One' ],
	[ 1, 'Two' ],
	[ 2, 'Three' ],
	[ 3, 'Four' ],
	[ 4, 'Five' ],
    ];
    makeOption("face_index", "Select face variant", faces.slice(0, num_faces));
}

makeOption("draw_mode", "Invert colors");

if ($.url().param("chrono_dial") != undefined) {
    makeOption("chrono_dial", "Bottom chrono dial shows",
	       [[0, 'Off'], [1, 'Tenths'], [2, 'Hours'], [3, 'Dual (tenths, then hours)']]);
}

// This list is duplicated in resources/make_lang.py.
var langs = [
    [ 'ar_SA', 'Arabic', 'rtl' ],
    [ 'hy_AM', 'Armenian', 'extended' ],
    [ 'zh_CN', 'Chinese', 'cjk' ],
    [ 'cs_CZ', 'Czech', 'latin' ],
    [ 'da_DK', 'Danish', 'latin' ],
    [ 'nl_NL', 'Dutch', 'latin' ],
    [ 'en_US', 'English', 'latin' ],
    [ 'fa_IR', 'Farsi', 'rtl' ],
    [ 'fr_FR', 'French', 'latin' ],
    [ 'de_DE', 'German', 'latin' ],
    [ 'el_GR', 'Greek', 'extended' ],
    [ 'he_IL', 'Hebrew', 'rtl' ],
    [ 'hi_IN', 'Hindi', 'extended' ],
    [ 'hu_HU', 'Hungarian', 'latin' ],
    [ 'is_IS' ,'Icelandic', 'latin' ],
    [ 'it_IT', 'Italian', 'latin' ],
    [ 'ja_JP', 'Japanese', 'cjk' ],
    [ 'ko_KR', 'Korean', 'cjk' ],
    [ 'pl_PL', 'Polish', 'latin' ],
    [ 'pt_PT', 'Portuguese', 'latin' ],
    [ 'ru_RU', 'Russian', 'extended' ],
    [ 'es_ES', 'Spanish', 'latin' ],
    [ 'sv_SE', 'Swedish', 'latin' ],
    [ 'tl', 'Tagalog', 'extended' ],
    [ 'ta_IN', 'Tamil', 'extended' ],
    [ 'th_TH', 'Thai', 'extended' ],
    [ 'tr_TR', 'Turkish', 'extended' ],
];

var date_window_options = [
    [0, 'Off'], 
    [1, 'Identify window'], 
    [2, 'Numeric date'],
    [4, 'Weekday'],
    [5, 'Month'],
    [3, 'Year'],
    [6, 'am/pm'],
];

if ($.url().param("support_moon")) {
    date_window_options.push([7, 'Lunar phase']);
}

var num_date_windows = $.url().param("num_date_windows");
if (num_date_windows) {
    for (var i = 0; i < num_date_windows; ++i) {
	var sym = 'date_window_' + String.fromCharCode(97 + i);
	var label = 'Date window ' + String.fromCharCode(65 + i);
	makeOption(sym, label, date_window_options);
    }

    makeOption("display_lang", "Language for date windows", langs, storeStringResult);
}

if ($.url().param("support_moon")) {
    makeOption("lunar_background", "Moon background color",
	       [[0, 'Same as watch background'], [1, 'Always black']]);
    makeOption("lunar_direction", "Moon phase direction",
	       [[0, 'Right-to-left (northern hemisphere)'], [1, 'Left-to-right (southern hemisphere)']]);
}

makeOption("hour_buzzer", "Vibrate at each hour");
makeOption("bluetooth_buzzer", "Vibrate on disconnect");
makeOption("bluetooth_indicator", "Connection indicator",
	   [[0, 'Off'], [1, 'When needed'], [2, 'Always']]);
makeOption("battery_gauge", "Battery gauge",
	   [[0, 'Off'], [1, 'When needed'], [2, 'Always'], [3, 'Digital']]);


function saveOptions() {
    var options = {
    };

    for (var ri in storeResults) {
	var keyword = storeResults[ri][0];
	var storeResult = storeResults[ri][1];
	storeResult(options, keyword);
    }
    return options;
}
