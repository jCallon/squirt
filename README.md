# squirt

Keep soil above a certain moisture.

An embedded C++ application written for ESP32 and FreeRTOS.

TODO: add GIF of it running and its healthy plant.

## Basic Flow

Use the screen and buttons to set:
- `desired_moisture`: The moisture content you want your soil to always be at or exceed. By default, this will be the level it read when it was turned on.
- `moisture_check_interval_minutes`: How often you want the device to check if the soil is beneath `desired_moisture`, in minutes. You may want to set this to more often when it's hot, for example.

To change a value:
1. Click the 'up' and 'down' buttons to navigate to the value you want to modify.
2. Click the 'confirm' button to start modifying the value.
3. Click the 'up' and 'down' buttons to increase or decrease the value.
4. Click the 'confirm' button to stop modifying the value.

Every `moisture_check_interval_minutes` minutes:
1. Check the moisture sensor.
2. If the moisture is below `desired_mositure`, add water by rotating the servo motor back and forth once, which squeezes the water bottle handle once.
3. Wait one second.
4. Repeat steps 2 and 3 until `desired_moisture` is reached or exceeded.

TODO: Can optimize by keeping track of how much one squirt increases the moisture level, calculate how many squirts should be needed to reach desired level, do that many squirts, then checking the mositure level and repeating. Saves the number of times the moisture sensor is fired.

## Parts

### Diagram

TODO: Add diagram showing layout and wiring of peripherals.

### List
| Quantity | Item | Purpose |
| -------- | ---- | ------- |
| 1 | [ESP32 Development Board](https://a.co/d/hLUOG6y) | Run and schedule decision-making logic. |
| 1 | [Servo Motor](https://a.co/d/i70ATR9) | Squeeze squeeze bottle handle. |
| 1 | [I2C LED Screen](https://a.co/d/aN8j0Sy) | Display current settings and latest readings. |
| 3 | [Button](https://a.co/d/3LTWaNc) | Give ability to modify settings. |
| 1 | [Soil Moisture Sensor](https://a.co/d/1c7H0MX) | Read the moisture level of the soil. |
| 1 | Squeeze bottle | Hold water and increase soil moisture level. |
| 1 | String | 'Attach' servo motor to squeeze bottle handle. |
| 1 | Rubber tube | Direct water from squeeze bottle to soil. |
