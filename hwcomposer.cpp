/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware/gralloc.h>

#include <gralloc_drm.h>
#include <gralloc_drm_priv.h>
#include <gralloc_drm_handle.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

struct hwc_context_t {
	hwc_composer_device_1 device;
	struct drm_module_t *gralloc_module;
};

static int hwc_device_open(const struct hw_module_t* module, const char* name,
		struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
	open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
	common: {
		tag: HARDWARE_MODULE_TAG,
		version_major: 1,
		version_minor: 0,
		id: HWC_HARDWARE_MODULE_ID,
		name: "Intel hwcomposer module",
		author: "Intel",
		methods: &hwc_module_methods,
	}
};


static int hwc_prepare(hwc_composer_device_1 *dev, size_t numDisplays,
	hwc_display_contents_1_t** displays)
{
	struct hwc_context_t* ctx = (struct hwc_context_t *) &dev->common;

	// SurfaceFlinger wants to handle the complete composition
	if (!displays[0]->hwLayers || displays[0]->numHwLayers == 0)
		return 0;

	int topmost = displays[0]->numHwLayers;
	if (displays[0]->numHwLayers > 0)
		topmost--;

	if (displays[0]->hwLayers->flags & HWC_GEOMETRY_CHANGED) {
		for (int i=topmost; i>=0; i--) {
			displays[0]->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
		}
	}
	return 0;
}


static int hwc_set(hwc_composer_device_1 *dev,
		size_t numDisplays, hwc_display_contents_1_t** displays)
{
	struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
	EGLBoolean success;

	// display is turning off
	if (!displays[0]->dpy)
		return 0;

	success = eglSwapBuffers((EGLDisplay)displays[0]->dpy,
		(EGLSurface)displays[0]->sur);

	if (!success)
		return HWC_EGL_ERROR;

	return 0;
}

// toggle display on or off
static int hwc_blank(struct hwc_composer_device_1* dev, int disp, int blank)
{
	// dummy implementation for now
	return 0;
}

// query number of different configurations available on display
static int hwc_get_display_cfgs(struct hwc_composer_device_1* dev, int disp,
	uint32_t* configs, size_t* numConfigs)
{
	// support just one config per display for now
	*configs = 1;
	*numConfigs = 1;

	return 0;
}

// query display attributes for a particular config
static int hwc_get_display_attrs(struct hwc_composer_device_1* dev, int disp,
	uint32_t config, const uint32_t* attributes, int32_t* values)
{
	int attr = 0;
	struct hwc_context_t* ctx = (struct hwc_context_t *) &dev->common;

	gralloc_drm_t *drm = ctx->gralloc_module->drm;

	// support only 1 display for now
	if (disp > 0)
		return -EINVAL;

	while(attributes[attr] != HWC_DISPLAY_NO_ATTRIBUTE) {
		switch (attr) {
			case HWC_DISPLAY_VSYNC_PERIOD:
				values[attr] = drm->primary.mode.vrefresh;
				break;
			case HWC_DISPLAY_WIDTH:
				values[attr] = drm->primary.mode.hdisplay;
				break;
			case HWC_DISPLAY_HEIGHT:
				values[attr] = drm->primary.mode.vdisplay;
				break;
			case HWC_DISPLAY_DPI_X:
				values[attr] = drm->primary.xdpi;
				break;
			case HWC_DISPLAY_DPI_Y:
				values[attr] = drm->primary.ydpi;
				break;
		}
		attr++;
	}
	return 0;
}

static int hwc_device_close(struct hw_device_t *dev)
{
	struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

	if (ctx)
		free(ctx);

	return 0;
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
		struct hw_device_t** device)
{
	int status = -EINVAL;
	if (strcmp(name, HWC_HARDWARE_COMPOSER))
		return status;

	struct hwc_context_t *dev;
	dev = (hwc_context_t*)calloc(1, sizeof(*dev));

	/* initialize the procs */
	dev->device.common.tag = HARDWARE_DEVICE_TAG;
	dev->device.common.version = 0;
	dev->device.common.module = const_cast<hw_module_t*>(module);
	dev->device.common.close = hwc_device_close;

	dev->device.prepare = hwc_prepare;
	dev->device.set = hwc_set;
	dev->device.blank = hwc_blank;
	dev->device.getDisplayAttributes = hwc_get_display_attrs;
	dev->device.getDisplayConfigs = hwc_get_display_cfgs;

	*device = &dev->device.common;

	int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
		(const hw_module_t **)&dev->gralloc_module);

	ALOGD("Intel hwcomposer module");

	return 0;
}
