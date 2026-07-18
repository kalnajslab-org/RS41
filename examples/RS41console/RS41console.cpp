// Interactive serial console for an RS41 module.

// Send 'h' on the serial console for the list of commands.

#include <RS41.h>

bool first_loop = true;
bool recondition = false;
RS41 rs41(RS41SERIAL, RS41EN);

// Sensor data is read and printed on this interval (ms). The loop runs
// freely between reads so console commands are serviced promptly. Settable
// at runtime with the 's' command.
unsigned long sensor_interval_ms = 2000;
unsigned long last_sensor_ms = 0;

// The set of parameters that can be displayed, in the canonical order of
// rs41.sensor_data_var_names. Populated once in setup().
#define MAX_PARAMS 32
String param_names[MAX_PARAMS];
int num_params = 0;

// The parameters currently selected for display via the 'p' command.
// num_selected == 0 means "display all parameters".
String selected_params[MAX_PARAMS];
int num_selected = 0;

// When true, the 'o' command shows an in-place orientation table (using
// ANSI/VT100 escape sequences) instead of the CSV parameter rows.
bool orientation_mode = false;

// Forward declarations
void build_param_names();
bool is_valid_param(const String& name);
String param_value(const RS41::RS41SensorData_t& data, const String& name);
void print_named_line(String names[], int n);
void print_sensor_values(const RS41::RS41SensorData_t& data);
void draw_orientation(const RS41::RS41SensorData_t& data);
void list_params();
void set_sample_interval(const String& arg);
void print_help();
void handle_console();

void setup()
{
  Serial.begin(115200);


  delay(3000);

  Serial.print("RS41console built: ");
  Serial.print(__DATE__);
  Serial.print(",");
  Serial.println(__TIME__);

  // Power cycle the RS41 module. It was powered on
  // on previously by the constructor, but that didn't power cycle it.
  pinMode(RS41EN,OUTPUT);
  digitalWrite(RS41EN, LOW);
  delay(100);
  digitalWrite(RS41EN, HIGH);

#ifdef RATSRS41
  pinMode(RS41EN, OUTPUT);
  digitalWrite(RS41EN, HIGH);
#endif


  // Build the list of displayable parameter names for the 'p' command.
  build_param_names();

  // Configure the serial port, power on the RS41, capture the banner, query the metadata.
  rs41.init();

  // Print the metadata captured during init(). Don't issue a fresh
  // read_meta_data() here: init() has already primed an RSD read, and
  // that pending sensor frame would collide with a new RMD query.
  Serial.println("RS41 meta data:" + rs41.meta_data());
}

// Split rs41.sensor_data_var_names into param_names[] once at startup.
void build_param_names()
{
  String s = rs41.sensor_data_var_names;
  num_params = 0;
  int start = 0;
  while (start <= (int)s.length() && num_params < MAX_PARAMS) {
    int comma = s.indexOf(',', start);
    if (comma == -1) {
      comma = s.length();
    }
    String tok = s.substring(start, comma);
    tok.trim();
    if (tok.length() > 0) {
      param_names[num_params++] = tok;
    }
    start = comma + 1;
  }
}

// True if name is one of the known displayable parameters.
bool is_valid_param(const String& name)
{
  for (int i = 0; i < num_params; i++) {
    if (param_names[i] == name) {
      return true;
    }
  }
  return false;
}

// Return the value of the named parameter as a String. The name is
// assumed valid (checked with is_valid_param). Keep this in sync with
// rs41.sensor_data_var_names.
String param_value(const RS41::RS41SensorData_t& data, const String& name)
{
  if (name == "frame_count")         return String(data.frame_count);
  if (name == "air_temp_degC")       return String(data.air_temp_degC);
  if (name == "humdity_percent")     return String(data.humdity_percent);
  if (name == "hsensor_temp_degC")   return String(data.hsensor_temp_degC);
  if (name == "pres_mb")             return String(data.pres_mb);
  if (name == "internal_temp_degC")  return String(data.internal_temp_degC);
  if (name == "module_status")       return String(data.module_status);
  if (name == "module_error")        return String(data.module_error);
  if (name == "pcb_supply_V")        return String(data.pcb_supply_V);
  if (name == "lsm303_temp_degC")    return String(data.lsm303_temp_degC);
  if (name == "pcb_heater_on")       return String(data.pcb_heater_on);
  if (name == "magX_mG")             return String(data.magX_mG);
  if (name == "magY_mG")             return String(data.magY_mG);
  if (name == "magZ_mG")             return String(data.magZ_mG);
  if (name == "accelX_mg")           return String(data.accelX_mg);
  if (name == "accelY_mg")           return String(data.accelY_mg);
  if (name == "accelZ_mg")           return String(data.accelZ_mg);
  if (name == "cal_active")          return String(data.cal_active);
  if (name == "roll_deg")            return String(data.roll_deg);
  if (name == "pitch_deg")           return String(data.pitch_deg);
  if (name == "heading_deg")         return String(data.heading_deg);
  if (name == "orientation_quality") return String(data.orientation_quality);
  return String("?");
}

// Print a comma-separated line of the given parameter names (a header).
void print_named_line(String names[], int n)
{
  for (int i = 0; i < n; i++) {
    Serial.print(names[i]);
    if (i < n - 1) {
      Serial.print(",");
    }
  }
  Serial.println();
}

// Print a comma-separated line of the currently selected parameter values.
void print_sensor_values(const RS41::RS41SensorData_t& data)
{
  String* names = (num_selected == 0) ? param_names : selected_params;
  int n = (num_selected == 0) ? num_params : num_selected;
  for (int i = 0; i < n; i++) {
    Serial.print(param_value(data, names[i]));
    if (i < n - 1) {
      Serial.print(",");
    }
  }
  Serial.println();
}

// Handle the 'p' command: select which parameters to display. With no
// argument, display all; otherwise a comma-separated list of names. An
// unknown name is an error and leaves the current selection unchanged.
void select_params(const String& arg)
{
  if (arg.length() == 0) {
    num_selected = 0;
    Serial.println("Displaying all parameters:");
    print_named_line(param_names, num_params);
    return;
  }

  // Validate every requested name before changing the selection.
  String names[MAX_PARAMS];
  int n = 0;
  int start = 0;
  while (start <= (int)arg.length()) {
    int comma = arg.indexOf(',', start);
    if (comma == -1) {
      comma = arg.length();
    }
    String tok = arg.substring(start, comma);
    tok.trim();
    if (tok.length() > 0) {
      if (!is_valid_param(tok)) {
        Serial.println("Unknown parameter: '" + tok + "'. Selection unchanged.");
        return;
      }
      if (n < MAX_PARAMS) {
        names[n++] = tok;
      }
    }
    start = comma + 1;
  }

  num_selected = n;
  for (int i = 0; i < n; i++) {
    selected_params[i] = names[i];
  }
  Serial.println("Displaying selected parameters:");
  print_named_line(selected_params, num_selected);
}

// Draw the magnetometer, accelerometer and orientation values as a table
// that updates in place. Uses ANSI/VT100 escape sequences: "\033[H" homes
// the cursor, "\033[K" erases to end of line, "\033[0J" erases below. This
// needs a real terminal (pio device monitor, PuTTY, screen); the Arduino
// IDE Serial Monitor ignores the escapes.
void draw_orientation(const RS41::RS41SensorData_t& data)
{
  char line[96];

  Serial.print("\033[H");   // cursor to top-left
  snprintf(line, sizeof(line),
           "RSS421 orientation  frame %u\n(enter 's <interval(s)>' or 'o' to exit)",
           data.frame_count);
  Serial.print(line); Serial.print("\033[K\r\n\033[K\r\n");

  Serial.print("               X         Y         Z\033[K\r\n");
  snprintf(line, sizeof(line), "  Mag    %9.1f %9.1f %9.1f   mG",
           data.magX_mG, data.magY_mG, data.magZ_mG);
  Serial.print(line); Serial.print("\033[K\r\n");
  snprintf(line, sizeof(line), "  Accel  %9.1f %9.1f %9.1f   mg",
           data.accelX_mg, data.accelY_mg, data.accelZ_mg);
  Serial.print(line); Serial.print("\033[K\r\n\033[K\r\n");
  // Roll is rotation about X, pitch about Y, heading about Z (ICD 6.1).
  snprintf(line, sizeof(line), "         %9s %9s %9s",
           "roll", "pitch", "heading");
  Serial.print(line); Serial.print("\033[K\r\n");
  snprintf(line, sizeof(line), "  Angle  %9.2f %9.2f %9.2f   deg",
           data.roll_deg, data.pitch_deg, data.heading_deg);
  Serial.print(line); Serial.print("\033[K\r\n\033[K\r\n");

  snprintf(line, sizeof(line), "  Quality  %8.2f", data.orientation_quality);
  Serial.print(line); Serial.print("\033[K\r\n\033[K\r\n");

  Serial.print("  meta: "); Serial.print(rs41.meta_data()); Serial.print("\033[K\r\n");

  Serial.print("\033[0J");  // erase anything left below the table
}

// Print the selected parameters as a comma-separated list, suitable for
// use as a CSV header line.
void list_params()
{
  if (num_selected == 0) {
    print_named_line(param_names, num_params);
  } else {
    print_named_line(selected_params, num_selected);
  }
}

// Handle the 's' command: set the sample interval in seconds. With no
// argument, report the current interval.
void set_sample_interval(const String& arg)
{
  // In orientation mode the console output is clobbered by the table
  // redraw, so suppress it; the changed refresh rate is the feedback.
  if (arg.length() == 0) {
    if (!orientation_mode) {
      Serial.print("Sample interval: ");
      Serial.print(sensor_interval_ms / 1000.0);
      Serial.println(" s");
    }
    return;
  }
  float secs = arg.toFloat();
  if (secs <= 0.0) {
    if (!orientation_mode) {
      Serial.println("Invalid interval '" + arg + "'. Enter a positive number of seconds.");
    }
    return;
  }
  sensor_interval_ms = (unsigned long)(secs * 1000.0);
  if (!orientation_mode) {
    Serial.print("Sample interval set to ");
    Serial.print(secs);
    Serial.println(" s");
  }
}

// Print the list of console commands.
void print_help()
{
  Serial.println();
  Serial.println("RS41console commands:");
  Serial.println("  h - print this help");
  Serial.println("  l - print the selected parameters as a CSV header line");
  Serial.println("  o - toggle the in-place orientation table (needs a VT100 terminal)");
  Serial.println("  r - start RH reconditioning");
  Serial.println("  p - display all parameters");
  Serial.println("  p <name>,<name>,... - display only the listed parameters");
  Serial.println("  s - report the sample interval");
  Serial.println("  s <seconds> - set the sample interval");
  Serial.println();
}

// Read a command from the console (if one is waiting) and act on it.
// Non-blocking: returns immediately when no input is available.
void handle_console()
{
  if (Serial.available() == 0) {
    return;
  }
  String cmd = Serial.readString();
  cmd.trim();
  if (cmd.length() == 0) {
    return;
  }
  switch (cmd[0]) {
    case 'h':
      print_help();
      break;
    case 'l':
      list_params();
      break;
    case 'r':
      recondition = true;
      Serial.println("RH reconditioning requested.");
      break;
    case 'o':
      orientation_mode = !orientation_mode;
      Serial.print("\033[2J\033[H");   // clear the screen on entry and exit
      if (!orientation_mode) {
        Serial.println("Orientation mode off.");
        list_params();   // reprint the CSV header for the resumed rows
      }
      break;
    case 's': {
      String arg = cmd.substring(1);
      arg.trim();
      set_sample_interval(arg);
      break;
    }
    case 'p': {
      String arg = cmd.substring(1);
      arg.trim();
      select_params(arg);
      break;
    }
    default:
      Serial.println("Unknown command '" + cmd + "'. Enter 'h' for help.");
      break;
  }
}

void loop()
{
  if (first_loop) {
    Serial.println(rs41.banner());
    Serial.println("Enter 'h' for command help.");
    Serial.println(rs41.sensor_data_var_names);
    first_loop = false;
    // Wait one interval before the first read so we don't miss the
    // first data frame.
    last_sensor_ms = millis();
  }

  // Act on any console command the user has typed.
  handle_console();

  // Only read and print sensor data once per interval; return to the top
  // of the loop otherwise so console commands stay responsive.
  if (millis() - last_sensor_ms < sensor_interval_ms) {
    return;
  }
  last_sensor_ms = millis();

  if (recondition) {
    String recond = rs41.recondition();
    if (!recond.length()){
      Serial.println("RS41 did not respond to RHS");
    } else {
      Serial.print("Recondition: ");
      Serial.println(recond);
      recondition = false;
    }
  }

  RS41::RS41SensorData_t sensor_data = rs41.decoded_sensor_data(false);
  if (sensor_data.valid) {
    if (orientation_mode) {
      draw_orientation(sensor_data);
    } else {
      print_sensor_values(sensor_data);
    }
  } else {
    Serial.println("Unable to obtain RS41 sensor data");
  }
}
