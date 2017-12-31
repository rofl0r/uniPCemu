#ifndef LOG_H
#define LOG_H

void initlog(); //Init log info!
void dolog(char *filename, const char *format, ...); //Logging functionality!
byte log_logtimestamp(byte logtimestamp); //Set/get the timestamp logging setting. 0/1=Set, 2+=Get only. Result: Old timestamp setting.
void closeLogFile(byte islocked); //Are we closing the log file?

#endif