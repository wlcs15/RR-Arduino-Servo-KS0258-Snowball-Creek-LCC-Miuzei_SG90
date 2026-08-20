#ifndef RR_SERVO_UNITY_CONFIG_H
#define RR_SERVO_UNITY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif
void rr_unity_putc(int c);
void rr_unity_flush(void);
#ifdef __cplusplus
}
#endif

#define UNITY_OUTPUT_CHAR(a) rr_unity_putc((int)(a))
#define UNITY_OUTPUT_FLUSH() rr_unity_flush()

#ifndef UNITY_EXCLUDE_FLOAT
#define UNITY_EXCLUDE_FLOAT
#endif

#endif
