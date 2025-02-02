/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

package lekkit.rvvm;

public class PCIBus {
    public PCIBus(RVVMMachine machine) {
        if (machine.isValid()) {
            RVVMNative.pci_bus_init_auto(machine.getPtr());
        }
    }
}
