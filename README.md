# module\_pidx

A sample [PIDX](https://github.com/sci-visus/PIDX) app for OSPRay, based
on the distributed sample app included with OSPRay.

## Building

Clone the repo into your OSPRay modules directory, then run CMake to build
OSPRay and pass `-DOSPRAY_MODULE_PIDX=ON` to build the module's movie
renderer and render workers and `-DOSPRAY_MODULE_PIDX_VIEWER=ON` to build
the remote viewer client. PIDX and TurboJPEG 1.5.x+ are required, along
with MPI. You can pass `-DTURBOJPEG_DIR` to the root directory of your
TurboJPEG installation directory if it's not installed in a standard location.

If you want to use the optional [transfer function widget](https://github.com/wilsonCernWq/TransferFunctionModule),
you can either [download](https://github.com/wilsonCernWq/TransferFunctionModule/archive/master.zip) or clone `git clone https://github.com/wilsonCernWq/TransferFunctionModule.git` repository to a place you like, and pass `-DTFN_MODULE_ROOT` to the root directory of it.

## Running the Remote Viewer

To run the remote viewer first start the render workers on your compute nodes.

```bash
mpirun -np <N> ./pidx_render_worker \
       -dataset <dataset.idx> \
       -port <port to listen on> \
       -timestep <timestep to load> \
       -variable <variable-name>
```

You can also pass the PIDX data directory to the `-dataset` variable. The program will
figure out which timestep to load automatically. The data directory should have the 
following structure:

```
folder/
    t0000001/
        l0/
            CCVars.idx
            CCVars/
            <etc...>
    t0000005/
        l0/
            CCVars.idx
            CCVars/
            <etc...>
```

These workers will start and load the data. Once they're ready to connect to
with the viewer, rank 0 will print out "Rank 0 now listening for client". You
can then start the viewer and pass it the hostname of rank 0 and the port
to connect to.

```bash
./pidx_viewer -server <rank 0 hostname> -port <port to connect>
```
