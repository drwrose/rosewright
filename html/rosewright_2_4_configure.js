function makeOption(keyword, label, options) {
    var role = "slider";
    if (options) {
	role = "select";
    } else {
	options = [[0, 'Off'], [1, 'On']];
    }
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

makeOption("keep_battery_gauge", "Keep battery visible");
makeOption("keep_bluetooth_indicator", "Keep bluetooth visible");
makeOption("second_hand", "Second hand");

if ($.url().param("sweep_seconds") != undefined) {
    makeOption("sweep_seconds", "Smooth-sweep seconds (heavy battery use)");
}	

makeOption("hour_buzzer", "Vibrate at each hour");
makeOption("draw_mode", "Invert colors");

if ($.url().param("chrono_dial") != undefined) {
    makeOption("chrono_dial", "Bottom chrono dial shows",
	       [[0, 'Off'], [1, 'Tenths'], [2, 'Hours'], [3, 'Dual (tenths, then hours)']]);
}

// This list is duplicated in resources/make_lang.py.
var langs = [
    [ 'en_US', 'English' ],
    [ 'fr_FR', 'French' ],
    [ 'it_IT', 'Italian' ],
    [ 'es_ES', 'Spanish' ],
    [ 'pt_PT', 'Portuguese' ],
    [ 'de_DE', 'German' ],
    [ 'nl_NL', 'Dutch' ],
    [ 'da_DK', 'Danish' ],
    [ 'sv_SE', 'Swedish' ],
    [ 'is_IS' ,'Icelandic' ],
    [ 'el_GR', 'Greek' ],
    [ 'hu_HU', 'Hungarian' ],
    [ 'ru_RU', 'Russian' ],
    [ 'pl_PL', 'Polish' ],
    [ 'cs_CZ', 'Czech' ],
];

if ($.url().param("show_day") != undefined) {
    makeOption("show_day", "Show day of week");
    makeOption("display_lang", "Language for day", langs);
}

if ($.url().param("show_date") != undefined) {
    makeOption("show_date", "Show numeric date");
}

function saveOptions() {
  var options = {
    'keep_battery_gauge': parseInt($("#keep_battery_gauge").val(), 10),
    'keep_bluetooth_indicator': parseInt($("#keep_bluetooth_indicator").val(), 10),
    'second_hand': parseInt($("#second_hand").val(), 10),
    'hour_buzzer': parseInt($("#hour_buzzer").val(), 10),
    'draw_mode': parseInt($("#draw_mode").val(), 10),
    'chrono_dial': parseInt($("#chrono_dial").val(), 10),
    'sweep_seconds': parseInt($("#sweep_seconds").val(), 10),
    'show_day': parseInt($("#show_day").val(), 10),
    'show_date': parseInt($("#show_date").val(), 10),
    'display_lang': $("#display_lang").val(),
  }
  return options;
}
