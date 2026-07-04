// Send sensor data from the RS41 to the serial console 
// and save to a CSV file.

#include <RS41.h>
#include "SD.h"
#include "SPI.h"

// A new file is created every time that the program is run.
//
// The data file names will take the form of:
// DATA_FILE_BASE_NAMEnnnnnDATA_FILE_EXT
// e.g. RS41_data_00092.csv

#define DATA_FILE_BASE_NAME "RS41_data_"
#define DATA_FILE_EXT ".csv"

bool verbose = true;
bool first_loop = true;
RS41 rs41(RS41SERIAL, RS41EN);

// The csv file to create. It will be empty
// if we are unable to access the SD card.
String csv_file_name = "";
File csv_file;

// Forward declarations
String next_file_name();
boolean isIntStr(String str);

void setup()
{
  Serial.begin(115200);
  delay(3000);
  while (!Serial) {;}  
  
  Serial.print("RS41csv built: ");
  Serial.print(__DATE__);
  Serial.print(",");
  Serial.println(__TIME__);

  if (!SD.begin(BUILTIN_SDCARD))
  {
    Serial.println("Unable to acces the SD card, data WILL NOT be saved to a file");
  }
  else
  {
    csv_file_name = next_file_name();
    if (verbose)
    {
      Serial.println("Creating " + String(csv_file_name));
      csv_file = SD.open(csv_file_name.c_str(), O_WRITE);
    }
  }

  // Configure the RS41 serial port, power on the RS41, capture the banner, query the metadata.
  rs41.init();
}

String comma(',');

void loop()
{
  if (first_loop)
    {
      if (csv_file_name.length()){
        csv_file.write(rs41.sensor_data_var_names.c_str(), strlen(rs41.sensor_data_var_names.c_str()));
        csv_file.write("\n",1);
        csv_file.flush();
    }
    Serial.println(rs41.sensor_data_var_names);
    first_loop = false;
  }

  // A delay at the start prevents missing the first data frame
  delay(2000);

  RS41::RS41SensorData_t sensor_data = rs41.decoded_sensor_data(false);
  if (sensor_data.valid)
  {
    // Build the csv string
    String csv_str =
        String(sensor_data.frame_count) + comma +
        String(sensor_data.air_temp_degC) + comma +
        String(sensor_data.humdity_percent) + comma +
        String(sensor_data.hsensor_temp_degC) + comma +
        String(sensor_data.pres_mb) + comma +
        String(sensor_data.internal_temp_degC) + comma +
        String(sensor_data.module_status) + comma +
        String(sensor_data.module_error) + comma +
        String(sensor_data.pcb_supply_V) + comma +
        String(sensor_data.lsm303_temp_degC) + comma +
        String(sensor_data.pcb_heater_on) + comma +
        String(sensor_data.magX_mG) + comma +
        String(sensor_data.magY_mG) + comma +
        String(sensor_data.magZ_mG) + comma +
        String(sensor_data.accelX_mg) + comma +
        String(sensor_data.accelY_mg) + comma +
        String(sensor_data.accelZ_mg) + comma +
        String(sensor_data.roll_deg) + comma +
        String(sensor_data.pitch_deg) + comma +
        String(sensor_data.heading_deg) + comma +
        String(sensor_data.orientation_quality);

    if (csv_file_name.length()) {
      // Write it to the file
      csv_file.write(csv_str.c_str(), csv_str.length());
      csv_file.write("\n",1);
      csv_file.flush();
    }

    // Print it
    Serial.println(csv_str);
  }
  else
  {
    Serial.println("Unable to obtain RS41 sensor data");
  }
}

String next_file_name()
{
  // Scan the root directory of the SD card, and
  // find the highest numbered data file. Return a
  // string with the next higher numbered file name.

  int next_file_num = 0;
  File dir = SD.open("/");
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      break;
    }

    String file_name = entry.name();
    if (file_name.startsWith(DATA_FILE_BASE_NAME) && file_name.endsWith(DATA_FILE_EXT))
    {
      String file_num = file_name.substring(strlen(DATA_FILE_BASE_NAME), file_name.indexOf(DATA_FILE_EXT));
      if (isIntStr(file_num))
      {
        int n = file_num.toInt();
        if (next_file_num <= n)
        {
          next_file_num = n + 1;
        }
      }
    }
    if (entry.isDirectory())
    {
    }
    else
    {
    }
    entry.close();
  }

  char cbuf[10];
  snprintf(cbuf, sizeof(cbuf), "%05d", next_file_num);
  String next_name = String(DATA_FILE_BASE_NAME) + String(cbuf) + String(DATA_FILE_EXT);
  return next_name;
}

boolean isIntStr(String str)
{
  for (unsigned int i = 0; i < str.length(); i++)
  {
    if (!isDigit(str.charAt(i)))
    {
      return false;
    }
  }
  return true;
}
