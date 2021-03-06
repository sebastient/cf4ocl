/*   
 * This file is part of cf4ocl (C Framework for OpenCL).
 * 
 * cf4ocl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cf4ocl is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with cf4ocl.  If not, see <http://www.gnu.org/licenses/>.
 * */
 
/** 
 * @file
 * @brief Bank conflicts test example. Control the level of conflicts 
 * using the stride `-s` parameter. 
 * 
 * The frequency of bank conflicts can be increased by doubling the 
 * stride `-s` parameter, e.g. 1, 2, 4, 16, 32. The maximum number of 
 * conflicts is obtained  with `s=16` or `s=32`, depending if the GPU 
 * has 16 or 32 banks of local memory.
 * 
 * @author Nuno Fachada
 * @date 2013
 * @copyright [GNU General Public License version 3 (GPLv3)](http://www.gnu.org/licenses/gpl.html)
 */

#include "exp_common.h"

/** Default global work size, dimension 0. */
#define GWS_X 4096
/** Default global work size, dimension 1. */
#define GWS_Y 4096
/** Default local work size, dimension 0. */
#define LWS_X 32
/** Default local work size, dimension 1. */
#define LWS_Y 16
/** Default stride. */
#define STRIDE 1

/** A description of the program. */
#define PROG_DESCRIPTION "Program for testing bank conflicts on the GPU"

/* Command line arguments and respective default values. */
static size_t gws[] = {GWS_X, GWS_Y};
static size_t lws[] = {LWS_X, LWS_Y};
static gchar* compiler_opts = NULL;
static int dev_idx = -1;
static int stride = STRIDE;

/* Callback functions to parse gws and lws. */
static gboolean bct_parse_gws(const gchar *option_name, const gchar *value, gpointer data, GError **err) {
	clexp_parse_pairs(value, gws, option_name, data, err);
}
static gboolean bct_parse_lws(const gchar *option_name, const gchar *value, gpointer data, GError **err) {
	clexp_parse_pairs(value, lws, option_name, data, err);
}

/* Valid command line options. */
static GOptionEntry entries[] = {
	{"compiler",   'c', 0, G_OPTION_ARG_STRING,   &compiler_opts, "Extra OpenCL compiler options",                                                             "OPTS"},
	{"globalsize", 'g', 0, G_OPTION_ARG_CALLBACK, bct_parse_gws,  "Work size (default is " STR(GWS_X) "," STR(GWS_Y) ")",                                      "SIZE,SIZE"},
	{"localsize",  'l', 0, G_OPTION_ARG_CALLBACK, bct_parse_lws,  "Local work size (default is " STR(LWS_X) "," STR(LWS_Y) ")",                                "SIZE,SIZE"},
	{"stride",     's', 0, G_OPTION_ARG_INT,      &stride,        "Stride (default is " STR(STRIDE) ")",                                                       "STRIDE"},
	{"device",     'd', 0, G_OPTION_ARG_INT,      &dev_idx,       "Device index (if not given and more than one device is available, chose device from menu)", "INDEX"},
	{ NULL, 0, 0, 0, NULL, NULL, NULL }	
};

/* Kernel file. */
static const char* kernelFiles[] = {"BankConflictTest.cl"};

/** 
 * @brief Bank conflicts test example main function.
 * 
 * The frequency of bank conflicts can be increased by doubling the 
 * stride `-s` parameter, e.g. 1, 2, 4, 16, 32. The maximum number of 
 * conflicts is obtained  with `s=16` or `s=32`, depending if the GPU 
 * has 16 or 32 banks of local memory.
 * 
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return #CLEXP_SUCCESS if program returns with no error, or 
 * #CLEXP_FAIL otherwise.
 * */
int main(int argc, char *argv[])
{

	/* ***************** */	
	/* Program variables */
	/* ***************** */	
	
	int status;                        /* Function and program return status. */
	GError *err = NULL;                /* Error management. */
	ProfCLProfile* profile = NULL;     /* Profiling / Timmings. */
	cl_kernel kernel_bankconf = NULL;  /* Kernel. */
	cl_event events[2] = {NULL, NULL}; /* Events. */
	cl_mem data_device = NULL;         /* Data in device. */
	cl_int *data_host = NULL;          /* Data in host. */
	CLUZone* zone = NULL;              /* OpenCL zone. */
	size_t sizeDataInBytes;            /* Size of data to be transfered to device. */
	size_t localMemSizeInBytes;        /* Size of local memory required. */
	GOptionContext* context = NULL;    /* Command line options context. */
	GRand* rng = NULL;	               /* Random number generator. */
	
	/* ************************** */
	/* Parse command line options */
	/* ************************** */

	/* Create parsing context. */
	context = g_option_context_new (" - " PROG_DESCRIPTION);
	/* Add acceptable command line options to context. */ 
	g_option_context_add_main_entries(context, entries, NULL);
	/* Use context to parse command line options. */
	g_option_context_parse(context, &argc, &argv, &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);
	/* Check if global worksize is multiple of local worksize. */
	gef_if_error_create_goto(err, CLEXP_ERROR, (gws[0] % lws[0] != 0) || (gws[1] % lws[1] != 0), CLEXP_FAIL, error_handler, "Global worksize is not multiple of local worksize.");
	
	/* ******************************************************* */
	/* Initialize profiler, OpenCL variables and build program */
	/* ******************************************************* */
	
	/* Initialize RNG. */
	rng = g_rand_new_with_seed(0);
		
	/* Initialize profiling object. */
	profile = profcl_profile_new();
	gef_if_error_create_goto(err, CLEXP_ERROR, profile == NULL, CLEXP_FAIL, error_handler, "Unable to create profiler object.");
	
	/* Get the required CL zone. */
	zone = clu_zone_new(CL_DEVICE_TYPE_GPU, 1, CL_QUEUE_PROFILING_ENABLE, clu_menu_device_selector, (dev_idx != -1 ? &dev_idx : NULL), &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);
	
	/* Build program. */
	status = clu_program_create(zone, kernelFiles, 1, compiler_opts, &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);

	/* Kernel */
	kernel_bankconf = clCreateKernel(zone->program, "bankconf", &status);
	gef_if_error_create_goto(err, CLEXP_ERROR, CL_SUCCESS != status, CLEXP_FAIL, error_handler, "OpenCL error %d: unable to create bankconf kernel.", status);

	/* Start basic timming / profiling. */
	profcl_profile_start(profile);

	/* ********************************** */	
	/* Create and initialize host buffers */
	/* ********************************** */	
	
	/* Data in host */
	sizeDataInBytes = gws[0] * gws[1] * sizeof(cl_int);
	data_host = (cl_int*) malloc(sizeDataInBytes);
	gef_if_error_create_goto(err, CLEXP_ERROR, data_host == NULL, CLEXP_FAIL, error_handler, "Unable to allocate memory for host data.");
	for (unsigned int i = 0; i < gws[0] * gws[1]; i++)
			data_host[i] = g_rand_int_range(rng, 0, 25);

	/* ********************* */
	/* Create device buffers */
	/* ********************* */

	/* Data in device */
	data_device = clCreateBuffer(zone->context, CL_MEM_READ_WRITE, sizeDataInBytes, NULL, &status);
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d: unable to create device buffer.", status);
	
	/* ************************* */
	/* Initialize device buffers */
	/* ************************* */
	
	status = clEnqueueWriteBuffer(zone->queues[0], data_device, CL_TRUE, 0, sizeDataInBytes, data_host, 0, NULL, &(events[0]));
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d: unable to write data to device.", status);

	/* ************************************************** */
	/* Determine and print required memory and work sizes */
	/* ************************************************** */

	localMemSizeInBytes = lws[1] * lws[0] * sizeof(cl_int);
	clexp_reqs_print(gws, lws, sizeDataInBytes, localMemSizeInBytes);

	/* *************************** */
	/*  Set fixed kernel arguments */
	/* *************************** */

	status = clSetKernelArg(kernel_bankconf, 0, sizeof(cl_mem), (void *) &data_device);
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d: unable set arg. 0 of bankconf kernel.", status);

	status = clSetKernelArg(kernel_bankconf, 1, localMemSizeInBytes, NULL);
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d: unable set arg. 1 of bankconf kernel.", status);
	
	status = clSetKernelArg(kernel_bankconf, 2, sizeof(cl_uint), (void *) &stride);
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d: unable set arg. 2 of bankconf kernel.", status);

	/* ************ */
	/*  Run kernel! */
	/* ************ */
	
	status = clEnqueueNDRangeKernel(zone->queues[0], kernel_bankconf, 2, NULL, gws, lws, 1, events, &(events[1]));
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d: unable to execute bankconf kernel.", status);

	status = clFinish(zone->queues[0]);
	gef_if_error_create_goto(err, CLEXP_ERROR, status != CL_SUCCESS, CLEXP_FAIL, error_handler, "OpenCL error %d in clFinish", status);

	/* ******************** */
	/*  Show profiling info */
	/* ******************** */
	
	profcl_profile_stop(profile); 

	profcl_profile_add(profile, "Transfer matrix A to device", events[0], &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);

	profcl_profile_add(profile, "Kernel execution (bankconf)", events[1], &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);

	profcl_profile_aggregate(profile, &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);

	profcl_print_info(profile, PROFCL_AGGEVDATA_SORT_TIME, &err);
	gef_if_error_goto(err, CLEXP_FAIL, status, error_handler);
	
	/* If we get here, no need for error checking, jump to cleanup. */
	g_assert (err == NULL);
	status = CLEXP_SUCCESS;
	goto cleanup;
	
	/* ************** */
	/* Error handling */
	/* ************** */
	
error_handler:
	/* If we got here there was an error, verify that it is so. */
	g_assert (err != NULL);
	fprintf(stderr, "Error %d from domain '%s' with message: \"%s\"\n", err->code, g_quark_to_string(err->domain), err->message);
	g_error_free(err);
	status = CLEXP_FAIL;

cleanup:
		
	/* *********** */
	/* Free stuff! */
	/* *********** */
	
	/* Free command line variables. */
	if (context) g_option_context_free(context);
	if (compiler_opts) g_free(compiler_opts);
	
	/* Free RNG */
	if (rng != NULL) g_rand_free(rng);
	
	/* Free profile */
	if (profile) profcl_profile_free(profile);
	
	/* Release events */
	if (events[0]) clReleaseEvent(events[0]);
	if (events[1]) clReleaseEvent(events[1]);
	
	/* Release OpenCL kernels */
	if (kernel_bankconf) clReleaseKernel(kernel_bankconf);
	
	/* Release OpenCL memory objects */
	if (data_device) clReleaseMemObject(data_device);

	/* Free OpenCL zone */
	if (zone) clu_zone_free(zone);

	/* Free host resources */
	if (data_host) free(data_host);
	
	/* Return status. */
	return status;

}

