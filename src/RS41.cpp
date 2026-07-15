// RS41.cpp
#include "Arduino.h"
#include "RS41.h"

RS41::RS41(HardwareSerialIMXRT& serial, int rs41_en_pin) : 
_serial(serial), 
_rs41_en_pin(rs41_en_pin) {
}

RS41::~RS41() {
  digitalWrite(_rs41_en_pin, LOW);
}

void RS41::init() {
  _serial.begin(56700);
  _serial.setTimeout(RS41_SERIAL_TIMEOUT_MS);

  // Increase serial buffer sizes for Teensy 4.1
  _serial.addMemoryForRead(&_rs41_rx_buffer, sizeof(_rs41_rx_buffer));
  _serial.addMemoryForWrite(&_rs41_tx_buffer, sizeof(_rs41_tx_buffer));

  // Power cycle the RS41
  pinMode(_rs41_en_pin, OUTPUT);
  digitalWrite(_rs41_en_pin, LOW);
  delay(100);
  digitalWrite(_rs41_en_pin, HIGH);
  delay(1000); // Wait for RS41 banner to get sent

  // The RS41 immediately sends out a banner
  for (int i = 0; i < RS41_SERIAL_TRIES; i++) {
      String txt = _serial.readStringUntil('\r');
      if (txt.indexOf("NCAR") != -1) {
        _banner = txt;
        break;
    }
  }
  clear_read_buffer();

  // Wait for RS41 to get running. Without this delay, the
  // RS41 does not seem to respond to the RMD command. Don't know why.
  delay(1000);

  // Get the meta data
  for (int i = 0; i < RS41_SERIAL_TRIES; i++) {
      _meta = read_meta_data();
      if (_meta.length() != 0) {
        break;
    }
  }
  clear_read_buffer();

  // Send the initial read sensor data
  _serial.write("RSD");
  _serial.write("\r");
  _serial.flush();

}

void RS41::pwr_off() {
  // Power off the RS41
  pinMode(_rs41_en_pin, OUTPUT);
  digitalWrite(_rs41_en_pin, LOW);
}

String RS41::banner() {
  return _banner;
}

String RS41::meta_data() {
  return _meta;
}

RS41::RS41SensorData_t RS41::decoded_sensor_data(bool nocache = false)
{
  RS41SensorData_t decoded_data;
  decoded_data.valid = false;

  String str_data = read_sensor_data(nocache);
  if (str_data.length())
  {
    // The v3 RSS421 record has 18 tokens (indices 0-17).
    String tokens[18];
    // Add a trailing comma so that all tokens are terminated.
    str_data += ',';

    // for v3 RSS421
    if (tokenize_string(str_data, tokens, 18))
    {
      decoded_data.valid = true;
      decoded_data.frame_count = tokens[0].toInt();
      decoded_data.air_temp_degC = tokens[1].toFloat();
      decoded_data.humdity_percent = tokens[2].toFloat();
      decoded_data.hsensor_temp_degC = tokens[3].toFloat();
      decoded_data.pres_mb = tokens[4].toFloat();
      decoded_data.internal_temp_degC = tokens[5].toFloat();
      decoded_data.module_status = tokens[6].toInt();
      decoded_data.module_error = tokens[7].toInt();
      decoded_data.pcb_supply_V = tokens[8].toFloat();
      decoded_data.lsm303_temp_degC = tokens[9].toFloat();
      decoded_data.pcb_heater_on = tokens[10].toInt();
      decoded_data.magX_mG = tokens[11].toFloat();
      decoded_data.magY_mG = tokens[12].toFloat();
      decoded_data.magZ_mG = tokens[13].toFloat();
      decoded_data.accelX_mg = tokens[14].toFloat();
      decoded_data.accelY_mg = tokens[15].toFloat();
      decoded_data.accelZ_mg = tokens[16].toFloat();
      decoded_data.cal_active = tokens[17].toInt();
    }
    // for the middle version on the RSS421
    else if (tokenize_string(str_data, tokens, 14))
    {
      decoded_data.valid = true;
      decoded_data.frame_count = tokens[0].toInt();
      decoded_data.air_temp_degC = tokens[1].toFloat();
      decoded_data.humdity_percent = tokens[2].toFloat();
      decoded_data.hsensor_temp_degC = tokens[3].toFloat();
      decoded_data.pres_mb = tokens[4].toFloat();
      decoded_data.internal_temp_degC = tokens[5].toFloat();
      decoded_data.module_status = tokens[6].toInt();
      decoded_data.module_error = tokens[7].toInt();
      decoded_data.pcb_supply_V = tokens[8].toFloat();
      decoded_data.lsm303_temp_degC = tokens[9].toFloat();
      decoded_data.pcb_heater_on = tokens[10].toInt();
      decoded_data.magX_mG = tokens[11].toFloat();
      decoded_data.magY_mG = tokens[12].toFloat();
      decoded_data.magZ_mG = tokens[13].toFloat();
      // decoded_data.accelX_mg = tokens[14].toFloat();
      // decoded_data.accelY_mg = tokens[15].toFloat();
      // decoded_data.accelZ_mg = tokens[16].toFloat();
    }
    // for the old version on the RSS421
    else if (tokenize_string(str_data, tokens, 17))
    {
      decoded_data.valid = true;
      decoded_data.frame_count = tokens[0].toInt();
      decoded_data.air_temp_degC = tokens[1].toFloat();
      decoded_data.humdity_percent = tokens[2].toFloat();
      decoded_data.hsensor_temp_degC = tokens[3].toFloat();
      decoded_data.pres_mb = tokens[4].toFloat();
      decoded_data.internal_temp_degC = tokens[5].toFloat();
      decoded_data.module_status = tokens[6].toInt();
      decoded_data.module_error = tokens[7].toInt();
      decoded_data.pcb_supply_V = tokens[8].toFloat();
      decoded_data.lsm303_temp_degC = tokens[9].toFloat();
      decoded_data.pcb_heater_on = tokens[10].toInt();
      decoded_data.magX_mG = tokens[11].toFloat();
      decoded_data.magY_mG = tokens[12].toFloat();
      decoded_data.magZ_mG = tokens[13].toFloat();
      decoded_data.accelX_mg = tokens[14].toFloat();
      decoded_data.accelY_mg = tokens[15].toFloat();
      decoded_data.accelZ_mg = tokens[16].toFloat();
    }

    // Fill roll_deg/pitch_deg/heading_deg/orientation_quality for any
    // successfully decoded record.
    if (decoded_data.valid)
    {
      compute_orientation(decoded_data);
    }
  }
  return decoded_data;
}

void RS41::compute_orientation(RS41SensorData_t & data)
{
      // Straight from the ICD section 6: use the raw magnetometer and
      // accelerometer axes as reported.
      const double Mx = data.magX_mG;
      const double My = data.magY_mG;
      const double Mz = data.magZ_mG;
      const double Ax = data.accelX_mg;
      const double Ay = data.accelY_mg;
      const double Az = data.accelZ_mg;

      // NOTE: on the module we tested, the IMU axes did NOT match the frame
      // the ICD formulas assume. Empirically, board X = -(sensor Y) and
      // board Y = -(sensor X), with Z unchanged, applied to BOTH the
      // accelerometer and magnetometer, gave correct roll/pitch/heading
      // (applying it to only one made the heading track roll). This remap is
      // left here, commented out, pending confirmation of the axis
      // convention (another module vs. ICD discrepancy):
      //   const double Mx = -data.magY_mG;
      //   const double My = -data.magX_mG;
      //   const double Mz =  data.magZ_mG;
      //   const double Ax = -data.accelY_mg;
      //   const double Ay = -data.accelX_mg;
      //   const double Az =  data.accelZ_mg;

      // Roll and pitch from the accelerometer (ICD 6.3). Units cancel in the
      // ratios, so raw mg/mG counts can be used directly.
      const double roll = atan2(Ay, Az);
      const double pitch = atan2(-Ax, sqrt(Ay * Ay + Az * Az));

      // Tilt-compensate the magnetometer into the horizontal plane, then
      // compute the magnetic heading (ICD 6.4).
      const double Mxh = Mx * cos(pitch) + My * sin(roll) * sin(pitch) + Mz * cos(roll) * sin(pitch);
      const double Myh = My * cos(roll) - Mz * sin(roll);
      double heading = atan2(-Myh, Mxh) * 180.0 / PI;
      if (heading < 0.0)
      {
        // Normalize to [0, 360) degrees.
        heading += 360.0;
      }

      data.roll_deg = roll * 180.0 / PI;
      data.pitch_deg = pitch * 180.0 / PI;
      data.heading_deg = heading;

      // Orientation quality factor Q (ICD 6.6). The roll/pitch tilt
      // compensation assumes the accelerometer measures only gravity (1g =
      // 1000 mg); Q falls as the total acceleration departs from 1g, so it
      // gates whether the computed orientation can be trusted.
      const double a_mag = sqrt(Ax * Ax + Ay * Ay + Az * Az);
      double q = 1.0 - fabs(a_mag - 1000.0) / 1000.0;
      if (q < 0.0)
      {
        q = 0.0;
      } // clamp to [0, 1]
      if (q > 1.0)
      {
        q = 1.0;
      }
      data.orientation_quality = q;
    }

    String RS41::read_sensor_data(bool nocache = false)
    {

      if (nocache)
      {
        // clear the read buffer and issue a new RSD command
        clear_read_buffer();
        _serial.write("RSD");
        _serial.write("\r");
      }
      // There should be a data record waiting for us
      String sensor_data = _serial.readStringUntil('\r');

      // Initiate the next read sensor data
      _serial.write("RSD");
      _serial.write("\r");

      return (sensor_data);
    }

    String RS41::read_meta_data()
    {
      // The RMD reply is a multi-line block, one line per subsystem,
      // each terminated by '\r'. rs41_cmd() would only return the first
      // line, so read lines here until the RS41 stops sending (an empty
      // read means readStringUntil() hit the serial timeout with no data).
      //
      // NOTE: the RMD output we actually get does NOT match the
      // documentation. On this (development) module it emits per-subsystem
      // status lines before the documented metadata line, e.g.:
      //   MCP9808   [$18]: PASS
      //   MMC5983MA [$30]: PASS
      //   LSM6DSOTR [$6A]: PASS  104Hz  Accel:+-2g  Gyro:+-250dps
      //   CAL: Not loaded (EEPROM empty)
      //   224954291,722.15.61,3.07,F,+0.00,+0.24,0,0   <- documented line
      // We filter the extra lines out below. TODO: bug Terry about the
      // discrepancy between the RMD output and the documentation.
      clear_read_buffer();
      _serial.write("RMD");
      _serial.write("\r");
      _serial.flush();

      String meta;
      while (true)
      {
        String line = _serial.readStringUntil('\r');
        line.trim();
        if (line.length() == 0)
        {
          break;
        }
        // Development-module lines that are not present on flight
        // hardware. Print them, but don't save them as meta data.
        if (line.startsWith("MCP9808") || line.startsWith("CAL") ||
            line.startsWith("MMC5983MA") || line.startsWith("LSM6DSOTR"))
        {
          Serial.println(line);
          continue;
        }
        if (meta.length() != 0)
        {
          meta += ";";
        }
        meta += line;
      }
      meta.trim();
      return meta;
    }

    String RS41::recondition()
    {
      clear_read_buffer();

      // Send the recondition command
      String rhs_response = rs41_cmd("RHS");

      // Initiate the next read sensor data
      _serial.write("RSD");
      _serial.write("\r");

      return rhs_response;
    }

    String RS41::rs41_cmd(const String &cmd)
    {
      // Note that the default timeout for serial reads is 1000ms.
      // This is generous for sending a command to the RS41, having
      // it process the command, and return the value.
      //
      // If the flush and read wait times are a problem, we
      // can implement a scheme where the command is sent,
      // and we return later to get get the result.
      _serial.write(cmd.c_str());
      _serial.write("\r");
      _serial.flush();
      String response = _serial.readStringUntil('\r');
      return response;
    }

    void RS41::clear_read_buffer()
    {
      while (_serial.available())
      {
        _serial.read();
      }
    }
    bool RS41::tokenize_string(String & source, String(&tokens)[], int nTokens)
    {
      int token_num = 0;
      bool valid = false;
      int start = 0;
      int end = source.indexOf(',', start);
      while (end != -1)
      {
        tokens[token_num] = source.substring(start, end);
        start = end + 1;
        end = source.indexOf(',', start);
        if (token_num == (nTokens-1)) {
          valid = true;
          break;
        }
        token_num++;
    }

    return valid;
}
