int snpe_input_as_rawfile(const char *name, const char *buffer_type, const char *dlc_path,
                          const char *input_path, const char *output_path);

//called from libvpx
#ifdef __cplusplus
extern "C" {
#endif
    int snpe_check_runtime();
    void * snpe_init_network(int runtime_int, int model);
    void snpe_execute_byte(void *wrapper, unsigned char *buffer, float *output_buffer,
                           int number_of_elements);
    void snpe_execute_float(void *wrapper, float *buffer, float * output_buffer, int number_of_elements);
    void snpe_free(void *wrapper);
#ifdef __cplusplus
}
#endif


