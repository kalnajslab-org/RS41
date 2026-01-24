// Test an RS41 module

// The user is prompted whether the RS41 should be reconditioned.

#include <RS41.h>

bool first_loop = true;
bool recondition = false;
RS41 rs41(RS41SERIAL, RS41EN);

void setup()
{
  Serial.begin(115200);
  delay(3000);

  Serial.print("RS41test built: ");
  Serial.print(__DATE__);
  Serial.print(",");
  Serial.println(__TIME__);

#ifdef RATSRS41
  pinMode(RS41EN, OUTPUT);
  digitalWrite(RS41EN, HIGH);
#endif


  Serial.println("Do you want to recondition the RS41[y/n]? ");
  while (Serial.available() == 0) {}
  String ans = Serial.readString().trim();
  if ((ans=="Y") || (ans=="y")) {
    recondition = true;
  }
  // Configure the serial port, power on the RS41, capture the banner, query the metadata.
  rs41.init();
}

void loop()
{
  if (first_loop) {
    Serial.println(rs41.banner());
    Serial.println(rs41.sensor_data_var_names);
    Serial.println("RS41 meta data:" + rs41.meta_data());
    first_loop = false;
  }

  // A delay at the start prevents missing the first data frame
  delay(2000);

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
    Serial.print(sensor_data.frame_count); Serial.print(",");
    Serial.print(sensor_data.air_temp_degC); Serial.print(",");
    Serial.print(sensor_data.humdity_percent); Serial.print(",");
    Serial.print(sensor_data.hsensor_temp_degC); Serial.print(",");
    Serial.print(sensor_data.pres_mb); Serial.print(",");
    Serial.print(sensor_data.internal_temp_degC); Serial.print(",");
    Serial.print(sensor_data.module_status); Serial.print(",");
    Serial.print(sensor_data.module_error); Serial.print(",");
    Serial.print(sensor_data.pcb_supply_V); Serial.print(",");
    Serial.print(sensor_data.lsm303_temp_degC); Serial.print(",");
    Serial.print(sensor_data.pcb_heater_on); Serial.print(",");
    Serial.print(sensor_data.mag_hdgXY_deg); Serial.print(",");
    Serial.print(sensor_data.mag_hdgXZ_deg); Serial.print(",");
    Serial.print(sensor_data.mag_hdgYZ_deg); Serial.print(",");
    Serial.print(sensor_data.accelX_mG); Serial.print(",");
    Serial.print(sensor_data.accelY_mG); Serial.print(",");
    Serial.print(sensor_data.accelZ_mG);
    Serial.println();
  } else {
    Serial.println("Unable to obtain RS41 sensor data");
  }
}
