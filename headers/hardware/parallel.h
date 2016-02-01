#ifndef PARALLEL_H
#define PARALLEL_H

typedef void (*ParallelOutputHandler)(byte data);
typedef void (*ParallelControlOUTHandler)(byte control);
typedef byte (*ParallelControlINHandler)();
typedef byte (*ParallelStatusHandler)();

void registerParallel(byte port, ParallelOutputHandler outputhandler, ParallelControlOUTHandler controlouthandler, ParallelControlINHandler controlinhandler, ParallelStatusHandler statushandler);
void setParallelIRQ(byte port, byte raised);
void tickParallel(double timepassed);
void initParallelPorts(byte numports);

#endif
