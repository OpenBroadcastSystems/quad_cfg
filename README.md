# quad_cfg
Sets up a Quad 2 card for 8 independent I/Os

Make sure SDK_PATH in the Makefile points to the SDK's include directory e.g.

`/home/user/Blackmagic\ DeckLink\ SDK\ 10.5.5/Linux/include`


To compile, run make after setting SDK_PATH

To run and set all devices to half duplex (e.g. single connector per device, for either input or output),

    `./quad_cfg`

To run and set all devices to full duplex (e.g. 2 connectors per device, one for input and one for output).

    `./quad_cfg full`
