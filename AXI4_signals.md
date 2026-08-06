| Signal | Source: Master / Slave | Input / Output | Description |
| :--- | :--- | :--- | :--- |
| **Aclk** | Global | Input | Global clock signal. |
| **AResetn** | Global | Input | Global reset signal. |
| **AWID[3:0]** | Master | Input | Write address ID. |
| **AWADDR[31:0]** | Master | Input | Write address. |
| **AWLEN[3:0]** | Master | Input | Write burst length. |
| **AWSIZE[2:0]** | Master | Input | Write burst size. |
| **AWBURST[1:0]** | Master | Input | Write burst type. |
| **AWLOCK[1:0]** | Master | Input | Write lock type. |
| **AWCACHE[3:0]** | Master | Input | Write cache type. |
| **AWPROT[2:0]** | Master | Input | Write protection type. |
| **WDATA[31:0]** | Master | Input | Write data. |
| **ARID[3:0]** | Master | Input | Read address ID. |
| **ARADDR[31:0]** | Master | Input | Read address. |
| **ARLEN[3:0]** | Master | Input | Read Burst length. |
| **ARSIZE[2:0]** | Master | Input | Read Burst size. |
| **ARLOCK[1:0]** | Master | Input | Read Lock type. |
| **ARCACHE[3:0]** | Master | Input | Read Cache type. |
| **ARPROT[2:0]** | Master | Input | Read Protection type. |
| **RDATA[31:0]** | Master | Input | Read data. |
| **WLAST** | Master | Input | Write last. |
| **RLAST** | Slave | Output | Read last. |
| **AWVALID** | Master | Output | Write address valid. |
| **AWREADY** | Slave | Output | Write address ready. |
| **WVALID** | Master | Output | Write valid. |
| **RAVLID** | Slave | Output | Read valid. |
| **WREADY** | Slave | Output | Write ready. |
| **BID[3:0]** | Slave | Output | Write Response ID. |
| **RID[3:0]** | Slave | Output | Read response ID. |
| **BRESP[1:0]** | Slave | Output | Write response. |
| **RRESP[1:0]** | Slave | Output | Read response. |
| **BVALID** | Slave | Output | Write response valid. |
| **BREADY** | Master | Output | Response ready. |
| **RVALID** | Slave | Output | Read valid. |
