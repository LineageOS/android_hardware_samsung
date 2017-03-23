/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _S3CFB_LCD_
#define _S3CFB_LCD_

#define S3C_FB_MAX_WIN	(5)

/*
 * S T R U C T U R E S  F O R  C U S T O M  I O C T L S
 *
*/
struct s3cfb_user_window {
    int x;
    int y;
};

struct s3cfb_user_plane_alpha {
    int             channel;
    unsigned char   red;
    unsigned char   green;
    unsigned char   blue;
};

struct s3cfb_user_chroma {
    int             enabled;
    unsigned char   red;
    unsigned char   green;
    unsigned char   blue;
};

typedef struct {
    unsigned int phy_start_addr;
    unsigned int xres;      /* visible resolution*/
    unsigned int yres;
    unsigned int xres_virtual;  /* virtual resolution*/
    unsigned int yres_virtual;
    unsigned int xoffset;   /* offset from virtual to visible */
    unsigned int yoffset;   /* resolution   */
    unsigned int lcd_offset_x;
    unsigned int lcd_offset_y;
} s3c_fb_next_info_t;

enum s3c_fb_pixel_format {
	S3C_FB_PIXEL_FORMAT_RGBA_8888 = 0,
	S3C_FB_PIXEL_FORMAT_RGB_888 = 1,
	S3C_FB_PIXEL_FORMAT_BGRA_8888 = 2,
	S3C_FB_PIXEL_FORMAT_RGB_565 = 3,
	S3C_FB_PIXEL_FORMAT_RGBX_8888 = 4,
	S3C_FB_PIXEL_FORMAT_RGBA_5551 = 5,
	S3C_FB_PIXEL_FORMAT_RGBA_4444 = 6,
	S3C_FB_PIXEL_FORMAT_MAX = 7,
};

enum s3c_fb_blending {
	S3C_FB_BLENDING_NONE = 0,
	S3C_FB_BLENDING_PREMULT = 1,
	S3C_FB_BLENDING_COVERAGE = 2,
	S3C_FB_BLENDING_MAX = 3,
};

struct s3c_fb_win_config {
	enum {
		S3C_FB_WIN_STATE_DISABLED = 0,
		S3C_FB_WIN_STATE_COLOR,
		S3C_FB_WIN_STATE_BUFFER,
	} state;

	union {
		__u32 color;
		struct {
			int	fd;
			__u32	phys_addr;
			__u32	virt_addr;
			__u32	offset;
			__u32	stride;
			enum s3c_fb_pixel_format format;
			enum s3c_fb_blending blending;
			int	fence_fd;
			int     plane_alpha;
		};
	};

	int	x;
	int	y;
	__u32	w;
	__u32	h;
};

struct s3c_fb_win_config_data {
	int	fence;
	struct s3c_fb_win_config config[S3C_FB_MAX_WIN];
};

#ifdef BOARD_USE_V4L2_ION
struct s3c_fb_user_ion_client {
    int fd;
    int offset;
};
#endif

/*
 * C U S T O M  I O C T L S
 *
*/

#define S3CFB_WIN_POSITION          _IOW ('F', 203, struct s3cfb_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA   _IOW ('F', 204, struct s3cfb_user_plane_alpha)
#define S3CFB_WIN_SET_CHROMA        _IOW ('F', 205, struct s3cfb_user_chroma)

#define S3CFB_SET_VSYNC_INT         _IOW ('F', 206, unsigned int)
#define S3CFB_GET_VSYNC_INT_STATUS  _IOR ('F', 207, unsigned int)

#define S3CFB_GET_ION_USER_HANDLE   _IOWR('F', 208, struct s3c_fb_user_ion_client)
#define S3CFB_WIN_CONFIG            _IOW ('F', 209, struct s3c_fb_win_config_data)

#define S3CFB_SET_SUSPEND_FIFO      _IOW ('F', 300, unsigned long)
#define S3CFB_SET_RESUME_FIFO       _IOW ('F', 301, unsigned long)

#define S3CFB_GET_LCD_WIDTH         _IOR ('F', 302, int)
#define S3CFB_GET_LCD_HEIGHT        _IOR ('F', 303, int)

#define S3CFB_SET_WRITEBACK         _IOW ('F', 304, unsigned int)
#define S3CFB_SET_WIN_ON            _IOW ('F', 305, unsigned int)
#define S3CFB_SET_WIN_OFF           _IOW ('F', 306, unsigned int)
#define S3CFB_SET_WIN_PATH          _IOW ('F', 307, enum s3cfb_data_path_t)
#define S3CFB_SET_WIN_ADDR          _IOW ('F', 308, unsigned long)
#define S3CFB_SET_WIN_MEM           _IOW ('F', 309, enum s3cfb_mem_owner_t)

#define S3CFB_GET_FB_PHY_ADDR       _IOR ('F', 310, unsigned int)
#define S3CFB_GET_CUR_WIN_BUF_ADDR  _IOR ('F', 311, unsigned int)
/*#if defined(CONFIG_CPU_EXYNOS4210)
#define S3CFB_SET_WIN_MEM_START		_IOW('F', 312, u32)
#endif*/

#define S3CFB_SET_ALPHA_MODE        _IOW ('F', 313, unsigned int)
#define S3CFB_SET_INITIAL_CONFIG    _IO  ('F', 314)
#define S3CFB_SUPPORT_FENCE         _IOW ('F', 315, unsigned int)


/* IOCTL commands from drivers/video/samsung_extdisp/s3cfb_extdsp.h */
struct s3cfb_extdsp_time_stamp {
	unsigned int		phys_addr;
	struct timeval		time_marker;
};

#define S3CFB_EXTDSP_SET_WIN_ADDR       _IOW('F', 308, unsigned long)
#define S3CFB_EXTDSP_GET_LOCKED_BUFFER  _IOW('F', 322, unsigned int)
#define S3CFB_EXTDSP_PUT_TIME_STAMP		_IOW('F', 323, \
						struct s3cfb_extdsp_time_stamp)

/***************** LCD frame buffer *****************/
#define FB0_NAME    "/dev/fb0"
#define FB1_NAME    "/dev/fb1"
#define FB2_NAME    "/dev/fb2"
#define FB3_NAME    "/dev/fb3"
#define FB4_NAME    "/dev/fb4"

#endif
