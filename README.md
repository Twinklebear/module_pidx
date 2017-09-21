# module\_pidx

A sample [PIDX](https://github.com/sci-visus/PIDX) app for OSPRay, based
on the distributed sample app included with OSPRay.

## Running the Remote Viewer

To run the remote viewer first start the render workers on your compute nodes.

```bash
mpirun -np <N> ./pidx_render_worker -dataset <dataset.idx> \
    -port <port to listen on> \
    -timestep <timestep to load
```

These workers will start and load the data. Once they're ready to connect to
with the viewer, rank 0 will print out "Rank 0 now listening for client". You
can then start the viewer and pass it the hostname of rank 0 and the port
to connect to.

```bash
./pidx_viewer -server <rank 0 hostname> -port <port to connect>
```

