This directory contains the code for the "Rosewright" family of watch faces.  Each of these consists basically of a bitmap face image, along with two or more bitmap hands, which are baked into the resource file at several pre-computed orientations, which are selected at runtime to easily and cleanly draw ornate animated watch hands.

You must configure a watch before you can compile this code!

Run the Python script config_watch.py in the root directory to configure a watch.  You must have the Python Imaging Library (PIL) installed to run this script successfully.  Use the command-line option -h to list the available options, or just use "-s a", "-s b", or "-s c" to select styles A, B, or C.

Once the watch is configured, you may use the waf tool to build it in the normal Pebble way.
