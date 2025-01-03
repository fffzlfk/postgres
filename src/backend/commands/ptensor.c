#include "commands/ptensor.h"
#include "postgres.h"

PTensor
AllocPTensor(int *dims, int len)
{
	PTensor		pt;
	int			size = 0;

	pt.dims = palloc0(len * sizeof(int));
	pt.dim_len = len;


	for (int i = 0; i < len; i++)
	{
		size *= dims[i];
		pt.dims[i] = dims[i];
	}

	pt.data = palloc0(size * sizeof(float));

	return pt;
}

void
FreePTensor(PTensor pt)
{
	pfree(pt.dims);
	pfree(pt.data);
}
