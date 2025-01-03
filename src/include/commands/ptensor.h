#ifndef PTENSOR_H
#define PTENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float *data;
  int *dims;
  int dim_len;
} PTensor;

PTensor
AllocPTensor(int *dims, int dim_len);

void
FreePTensor(PTensor pt);

#ifdef __cplusplus
}
#endif

#endif
