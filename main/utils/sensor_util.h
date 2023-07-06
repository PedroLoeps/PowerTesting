#pragma once

void sensors_init(void);
int infiltration_read(void);
float ph_sensor_read(int* code, int*volt);
void hum_temp_sensor_read(int* temp, int* hum);
bool water_level_read(void);