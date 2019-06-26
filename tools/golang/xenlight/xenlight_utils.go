/*
 * Copyright (C) 2019 Nicolas Belouin, Gandi SAS
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>.
 */
package xenlight

/*
#cgo LDFLAGS: -lxenlight -lyajl -lxentoollog
#include <stdlib.h>
#include <libxl_utils.h>
*/
import "C"

/*
 * Other flags that may be needed at some point:
 *  -lnl-route-3 -lnl-3
 *
 * To get back to static linking:
 * #cgo LDFLAGS: -lxenlight -lyajl_s -lxengnttab -lxenstore -lxenguest -lxentoollog -lxenevtchn -lxenctrl -lxenforeignmemory -lxencall -lz -luuid -lutil
 */

import (
	"unsafe"
)

//char* libxl_domid_to_name(libxl_ctx *ctx, uint32_t domid);
func (Ctx *Context) DomidToName(id Domid) (name string) {
	cDomName := C.libxl_domid_to_name(Ctx.ctx, C.uint32_t(id))
	name = C.GoString(cDomName)
	return
}

//int libxl_name_to_domid(libxl_ct *ctx, const char *name, uint32_t *domid)
func (Ctx *Context) NameToDomid(name string) (id Domid, err error) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	var cDomId C.uint32_t

	ret := C.libxl_name_to_domid(Ctx.ctx, cname, &cDomId)
	if ret != 0 {
		err = Error(-ret)
		return
	}

	id = Domid(cDomId)

	return
}
