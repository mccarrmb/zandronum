/*
** gl_material.cpp
** 
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "m_png.h"
#include "r_draw.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "stats.h"
#include "r_main.h"
#include "templates.h"
#include "sc_man.h"
#include "r_translate.h"
#include "colormatcher.h"

//#include "gl/gl_intern.h"

#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_bitmap.h"
#include "gl/textures/gl_material.h"
#include "gl/shaders/gl_shader.h"
#include "gl/utility/gl_convert.h"


EXTERN_CVAR(Bool, gl_render_precise)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool, gl_precache)
EXTERN_CVAR(Bool, gl_texture_usehires)

//===========================================================================
//
// The GL texture maintenance class
//
//===========================================================================

//===========================================================================
//
// Constructor
//
//===========================================================================
FGLTexture::FGLTexture(FTexture * tx, bool expandpatches)
{
	assert(tx->gl_info.SystemTexture == NULL);
	tex = tx;

	glpatch=NULL;
	for(int i=0;i<5;i++) gltexture[i]=NULL;
	HiresLump=-1;
	hirestexture = NULL;
	currentwarp = 0;
	bHasColorkey = false;
	bIsTransparent = -1;
	bExpand = expandpatches;
	tex->gl_info.SystemTexture = this;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FGLTexture::~FGLTexture()
{
	Clean(true);
	if (hirestexture) delete hirestexture;
}

//==========================================================================
//
// Checks for the presence of a hires texture replacement and loads it
//
//==========================================================================
unsigned char *FGLTexture::LoadHiresTexture(int *width, int *height, int cm)
{
	if (HiresLump==-1) 
	{
		bHasColorkey = false;
		HiresLump = CheckDDPK3(tex);
		if (HiresLump < 0) HiresLump = CheckExternalFile(tex, bHasColorkey);

		if (HiresLump >=0) 
		{
			hirestexture = FTexture::CreateTexture(HiresLump, FTexture::TEX_Any);
		}
	}
	if (hirestexture != NULL)
	{
		int w=hirestexture->GetWidth();
		int h=hirestexture->GetHeight();

		unsigned char * buffer=new unsigned char[w*(h+1)*4];
		memset(buffer, 0, w * (h+1) * 4);

		FGLBitmap bmp(buffer, w*4, w, h);
		bmp.SetTranslationInfo(cm);

		
		int trans = hirestexture->CopyTrueColorPixels(&bmp, 0, 0);
		hirestexture->CheckTrans(buffer, w*h, trans);
		bIsTransparent = hirestexture->gl_info.mIsTransparent;

		if (bHasColorkey)
		{
			// This is a crappy Doomsday color keyed image
			// We have to remove the key manually. :(
			DWORD * dwdata=(DWORD*)buffer;
			for (int i=(w*h);i>0;i--)
			{
				if (dwdata[i]==0xffffff00 || dwdata[i]==0xffff00ff) dwdata[i]=0;
			}
		}
		*width = w;
		*height = h;
		return buffer;
	}
	return NULL;
}

//===========================================================================
// 
//	Deletes all allocated resources
//
//===========================================================================

void FGLTexture::Clean(bool all)
{
	for(int i=0;i<5;i++)
	{
		if (gltexture[i]) 
		{
			if (!all) gltexture[i]->Clean(false);
			else
			{
				delete gltexture[i];
				gltexture[i]=NULL;
			}
		}
	}
	if (glpatch) 
	{
		if (!all) glpatch->Clean(false);
		else
		{
			delete glpatch;
			glpatch=NULL;
		}
	}
}

//===========================================================================
//
// FGLTex::WarpBuffer
//
//===========================================================================

BYTE *FGLTexture::WarpBuffer(BYTE *buffer, int Width, int Height, int warp)
{
	if (Width > 256 || Height > 256) return buffer;

	DWORD *in = (DWORD*)buffer;
	DWORD *out = (DWORD*)new BYTE[4*Width*Height];
	float Speed = static_cast<FWarpTexture*>(tex)->GetSpeed();

	static_cast<FWarpTexture*>(tex)->GenTime = r_FrameTime;

	static DWORD linebuffer[256];	// anything larger will bring down performance so it is excluded above.
	DWORD timebase = DWORD(r_FrameTime*Speed*23/28);
	int xsize = Width;
	int ysize = Height;
	int xmask = xsize - 1;
	int ymask = ysize - 1;
	int ds_xbits;
	int i,x;

	if (warp == 1)
	{
		for(ds_xbits=-1,i=Width; i; i>>=1, ds_xbits++);

		for (x = xsize-1; x >= 0; x--)
		{
			int yt, yf = (finesine[(timebase+(x+17)*128)&FINEMASK]>>13) & ymask;
			const DWORD *source = in + x;
			DWORD *dest = out + x;
			for (yt = ysize; yt; yt--, yf = (yf+1)&ymask, dest += xsize)
			{
				*dest = *(source+(yf<<ds_xbits));
			}
		}
		timebase = DWORD(r_FrameTime*Speed*32/28);
		int y;
		for (y = ysize-1; y >= 0; y--)
		{
			int xt, xf = (finesine[(timebase+y*128)&FINEMASK]>>13) & xmask;
			DWORD *source = out + (y<<ds_xbits);
			DWORD *dest = linebuffer;
			for (xt = xsize; xt; xt--, xf = (xf+1)&xmask)
			{
				*dest++ = *(source+xf);
			}
			memcpy (out+y*xsize, linebuffer, xsize*sizeof(DWORD));
		}
	}
	else
	{
		int ybits;
		for(ybits=-1,i=ysize; i; i>>=1, ybits++);

		DWORD timebase = (r_FrameTime * Speed * 40 / 28);
		for (x = xsize-1; x >= 0; x--)
		{
			for (int y = ysize-1; y >= 0; y--)
			{
				int xt = (x + 128
					+ ((finesine[(y*128 + timebase*5 + 900) & 8191]*2)>>FRACBITS)
					+ ((finesine[(x*256 + timebase*4 + 300) & 8191]*2)>>FRACBITS)) & xmask;
				int yt = (y + 128
					+ ((finesine[(y*128 + timebase*3 + 700) & 8191]*2)>>FRACBITS)
					+ ((finesine[(x*256 + timebase*4 + 1200) & 8191]*2)>>FRACBITS)) & ymask;
				const DWORD *source = in + (xt << ybits) + yt;
				DWORD *dest = out + (x << ybits) + y;
				*dest = *source;
			}
		}
	}
	delete [] buffer;
	return (BYTE*)out;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

unsigned char * FGLTexture::CreateTexBuffer(int _cm, int translation, int & w, int & h, bool expand, bool allowhires, int warp)
{
	unsigned char * buffer;
	intptr_t cm = _cm;
	int W, H;


	// Textures that are already scaled in the texture lump will not get replaced
	// by hires textures
	if (gl_texture_usehires && allowhires)
	{
		buffer = LoadHiresTexture (&w, &h, _cm);
		if (buffer)
		{
			return buffer;
		}
	}

	W = w = tex->GetWidth() + expand*2;
	H = h = tex->GetHeight() + expand*2;


	buffer=new unsigned char[W*(H+1)*4];
	memset(buffer, 0, W * (H+1) * 4);

	FGLBitmap bmp(buffer, W*4, W, H);
	bmp.SetTranslationInfo(cm, translation);

	if (tex->bComplex)
	{
		FBitmap imgCreate;

		// The texture contains special processing so it must be composited using the
		// base bitmap class and then be converted as a whole.
		if (imgCreate.Create(W, H))
		{
			memset(imgCreate.GetPixels(), 0, W * H * 4);
			int trans = tex->CopyTrueColorPixels(&imgCreate, expand, expand);
			bmp.CopyPixelDataRGB(0, 0, imgCreate.GetPixels(), W, H, 4, W * 4, 0, CF_BGRA);
			tex->CheckTrans(buffer, W*H, trans);
			bIsTransparent = tex->gl_info.mIsTransparent;
		}
	}
	else if (translation<=0)
	{
		int trans = tex->CopyTrueColorPixels(&bmp, expand, expand);
		tex->CheckTrans(buffer, W*H, trans);
		bIsTransparent = tex->gl_info.mIsTransparent;
	}
	else
	{
		// When using translations everything must be mapped to the base palette.
		// Since FTexture's method is doing exactly that by calling GetPixels let's use that here
		// to do all the dirty work for us. ;)
		tex->FTexture::CopyTrueColorPixels(&bmp, expand, expand);
		bIsTransparent = 0;
	}

	if (warp != 0)
	{
		buffer = WarpBuffer(buffer, W, H, warp);
	}
	// [BB] Potentially upsample the buffer. Note: Even is the buffer is not upsampled,
	// w, h are set to the width and height of the buffer.
	// Also don't upsample warped textures.
	else if (bIsTransparent != 1)
	{
		// [BB] Potentially upsample the buffer.
		buffer = gl_CreateUpsampledTextureBuffer ( tex, buffer, W, H, w, h, ( bIsTransparent == 1 ) || ( cm == CM_SHADE ) );
	}
	currentwarp = warp;
	currentwarptime = gl_frameMS;

	return buffer;
}


//===========================================================================
// 
//	Create hardware texture for world use
//
//===========================================================================

FHardwareTexture *FGLTexture::CreateTexture(int clampmode)
{
	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture
	if (!gltexture[clampmode]) 
	{
		gltexture[clampmode] = new FHardwareTexture(tex->GetWidth(), tex->GetHeight(), true, true);
	}
	return gltexture[clampmode]; 
}

//===========================================================================
// 
//	Create Hardware texture for patch use
//
//===========================================================================

bool FGLTexture::CreatePatch()
{
	if (tex->UseType==FTexture::TEX_Null) return false;		// Cannot register a NULL texture
	if (!glpatch) 
	{
		glpatch=new FHardwareTexture(tex->GetWidth() + bExpand, tex->GetHeight() + bExpand, false, false);
	}
	if (glpatch) return true; 	
	return false;
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const FHardwareTexture *FGLTexture::Bind(int texunit, int cm, int clampmode, int translation, bool allowhires, int warp)
{
	int usebright = false;

	if (translation <= 0) translation = -translation;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 8)) translation = CM_GRAY;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 7)) translation = CM_ICE;
	else translation = GLTranslationPalette::GetInternalTranslation(translation);

	FHardwareTexture *hwtex;
	
	if (gltexture[4] != NULL && clampmode < 4 && gltexture[clampmode] == NULL)
	{
		hwtex = gltexture[clampmode] = gltexture[4];
		gltexture[4] = NULL;

		if (hwtex->Bind(texunit, cm, translation))
		{
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (clampmode & GLT_CLAMPX)? GL_CLAMP_TO_EDGE : GL_REPEAT);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (clampmode & GLT_CLAMPY)? GL_CLAMP_TO_EDGE : GL_REPEAT);
		}
	}
	else
	{
		hwtex = CreateTexture(clampmode);
	}

	if (hwtex)
	{
		if ((warp != 0 || currentwarp != warp) && currentwarptime != gl_frameMS)
		{
			// must recreate the texture
			Clean(true);
			hwtex = CreateTexture(clampmode);
		}

		// Bind it to the system.
		if (!hwtex->Bind(texunit, cm, translation))
		{
			
			int w, h;

			// Create this texture
			unsigned char * buffer = NULL;
			
			if (!tex->bHasCanvas)
			{
				buffer = CreateTexBuffer(cm, translation, w, h, false, allowhires, warp);
				tex->ProcessData(buffer, w, h, false);
			}
			if (!hwtex->CreateTexture(buffer, w, h, true, texunit, cm, translation)) 
			{
				// could not create texture
				delete buffer;
				return NULL;
			}
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (clampmode & GLT_CLAMPX)? GL_CLAMP_TO_EDGE : GL_REPEAT);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (clampmode & GLT_CLAMPY)? GL_CLAMP_TO_EDGE : GL_REPEAT);
			delete buffer;
		}

		if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
		return hwtex; 
	}
	return NULL;
}

//===========================================================================
// 
//	Binds a sprite to the renderer
//
//===========================================================================
const FHardwareTexture * FGLTexture::BindPatch(int texunit, int cm, int translation, int warp)
{
	bool usebright = false;
	int transparm = translation;

	if (translation <= 0) translation = -translation;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 8)) translation = CM_GRAY;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 7)) translation = CM_ICE;
	else translation = GLTranslationPalette::GetInternalTranslation(translation);

	if (CreatePatch())
	{
		if (warp != 0 || currentwarp != warp)
		{
			// must recreate the texture
			Clean(true);
			CreatePatch();
		}

		// Bind it to the system. 
		if (!glpatch->Bind(texunit, cm, translation))
		{
			int w, h;

			// Create this texture
			unsigned char * buffer = CreateTexBuffer(cm, translation, w, h, bExpand, false, warp);
			tex->ProcessData(buffer, w, h, true);
			if (!glpatch->CreateTexture(buffer, w, h, false, texunit, cm, translation)) 
			{
				// could not create texture
				delete buffer;
				return NULL;
			}
			delete buffer;
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		return glpatch; 	
	}
	return NULL;
}

//===========================================================================
//
//
//
//===========================================================================
FGLTexture * FMaterial::ValidateSysTexture(FTexture * tex, bool expand)
{
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		FGLTexture *gltex = tex->gl_info.SystemTexture;
		if (gltex == NULL) 
		{
			gltex = new FGLTexture(tex, expand);
		}
		return gltex;
	}
	return NULL;
}

//===========================================================================
//
// Constructor
//
//===========================================================================
TArray<FMaterial *> FMaterial::mMaterials;
int FMaterial::mMaxBound;

FMaterial::FMaterial(FTexture * tx, bool forceexpand)
{
	assert(tx->gl_info.Material == NULL);

	bool expanded = tx->UseType == FTexture::TEX_Sprite || 
					tx->UseType == FTexture::TEX_SkinSprite || 
					tx->UseType == FTexture::TEX_Decal ||
					forceexpand;

	mShaderIndex = 0;


	// TODO: apply custom shader object here
	/* if (tx->CustomShaderDefinition)
	{
	}
	else
	*/
	if (tx->bWarped)
	{
		mShaderIndex = tx->bWarped;
		expanded = false;
		tx->gl_info.shaderspeed = static_cast<FWarpTexture*>(tx)->GetSpeed();
	}
	else if (gl.shadermodel > 2) 
	{
		if (tx->gl_info.shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->gl_info.shaderindex;
			expanded = false;
		}
		else
		{
			tx->CreateDefaultBrightmap();
			if (tx->gl_info.Brightmap != NULL)
			{
				ValidateSysTexture(tx->gl_info.Brightmap, expanded);
				FTextureLayer layer = {tx->gl_info.Brightmap, false};
				mTextureLayers.Push(layer);
				mShaderIndex = 3;
			}
		}
	}


	for (int i=GLUSE_PATCH; i<=GLUSE_TEXTURE; i++)
	{
		Width[i] = tx->GetWidth();
		Height[i] = tx->GetHeight();
		LeftOffset[i] = tx->LeftOffset;
		TopOffset[i] = tx->TopOffset;
		RenderWidth[i] = tx->GetScaledWidth();
		RenderHeight[i] = tx->GetScaledHeight();
	}

	tempScaleX = tempScaleY = FRACUNIT;
	wti.scalex = tx->xScale/(float)FRACUNIT;
	wti.scaley = tx->yScale/(float)FRACUNIT;
	if (tx->bHasCanvas) wti.scaley=-wti.scaley;

	FTexture *basetex;
	if (!expanded)
	{
		// check if the texture is just a simple redirect to a patch
		// If so we should use the patch for texture creation to
		// avoid eventual redundancies. For textures that need to
		// be expanded at the edges this may not be done though.
		// Warping can be ignored with SM4 because it's always done
		// by shader
		basetex = tx->GetRedirect(gl.shadermodel < 4);
	}
	else 
	{
		// a little adjustment to make sprites look better with texture filtering:
		// create a 1 pixel wide empty frame around them.
		basetex = tx;
		RenderWidth[GLUSE_PATCH]+=2;
		RenderHeight[GLUSE_PATCH]+=2;
		Width[GLUSE_PATCH]+=2;
		Height[GLUSE_PATCH]+=2;
		LeftOffset[GLUSE_PATCH]+=1;
		TopOffset[GLUSE_PATCH]+=1;
	}

	// make sure the system texture is valid
	mBaseLayer = ValidateSysTexture(basetex, expanded);

	mTextureLayers.ShrinkToFit();
	mMaxBound = -1;
	mMaterials.Push(this);
	tx->gl_info.Material = this;
	if (tx->bHasCanvas) tx->gl_info.mIsTransparent = 0;
	tex = tx;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FMaterial::~FMaterial()
{
	for(unsigned i=0;i<mMaterials.Size();i++)
	{
		if (mMaterials[i]==this) 
		{
			mMaterials.Delete(i);
			break;
		}
	}

}



//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const WorldTextureInfo *FMaterial::Bind(int cm, int clampmode, int translation)
{
	int usebright = false;
	int shaderindex = mShaderIndex;
	int maxbound = 0;
	bool allowhires = wti.scaley==1.0 && wti.scaley==1.0;

	int softwarewarp = gl_RenderState.SetupShader(tex->bHasCanvas, shaderindex, cm, tex->gl_info.shaderspeed);

	if (tex->bHasCanvas) clampmode = 0;
	else if (clampmode != -1) clampmode &= 3;
	else clampmode = 4;

	wti.gltexture = mBaseLayer->Bind(0, cm, clampmode, translation, allowhires, softwarewarp);
	if (wti.gltexture != NULL && shaderindex > 0)
	{
		for(unsigned i=0;i<mTextureLayers.Size();i++)
		{
			FTexture *layer;
			if (mTextureLayers[i].animated)
			{
				FTextureID id = mTextureLayers[i].texture->GetID();
				layer = TexMan(id);
				ValidateSysTexture(layer, false);
			}
			else
			{
				layer = mTextureLayers[i].texture;
			}
			layer->gl_info.SystemTexture->Bind(i+1, CM_DEFAULT, clampmode, 0, allowhires, false);
			maxbound = i+1;
		}
	}
	// unbind everything from the last texture that's still active
	for(int i=maxbound+1; i<=mMaxBound;i++)
	{
		FHardwareTexture::Unbind(i);
		mMaxBound = maxbound;
	}
	return &wti;
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const PatchTextureInfo * FMaterial::BindPatch(int cm, int translation)
{
	int usebright = false;
	int shaderindex = mShaderIndex;
	int maxbound = 0;

	int softwarewarp = gl_RenderState.SetupShader(tex->bHasCanvas, shaderindex, cm, tex->gl_info.shaderspeed);

	pti.glpatch = mBaseLayer->BindPatch(0, cm, translation, softwarewarp);
	// The only multitexture effect usable on sprites is the brightmap.
	if (pti.glpatch != NULL && shaderindex == 3)
	{
		mTextureLayers[0].texture->gl_info.SystemTexture->BindPatch(1, CM_DEFAULT, 0, 0);
		maxbound = 1;
	}
	// unbind everything from the last texture that's still active
	for(int i=maxbound+1; i<=mMaxBound;i++)
	{
		FHardwareTexture::Unbind(i);
		mMaxBound = maxbound;
	}
	return &pti;
}


//===========================================================================
//
//
//
//===========================================================================
void FMaterial::Precache()
{
	if (tex->UseType==FTexture::TEX_Sprite) 
	{
		BindPatch(CM_DEFAULT, 0);
	}
	else 
	{
		int cached = 0;
		for(int i=0;i<4;i++)
		{
			if (mBaseLayer->gltexture[i] != 0)
			{
				Bind (CM_DEFAULT, i, 0);
				cached++;
			}
			if (cached == 0) Bind(CM_DEFAULT, -1, 0);
		}
	}
}

//===========================================================================
// 
//
//
//===========================================================================

const WorldTextureInfo *FMaterial::GetWorldTextureInfo()
{
	for(int i=0;i<5;i++)
	{
		if (mBaseLayer->gltexture[i])
		{
			wti.gltexture = mBaseLayer->gltexture[i];
			return &wti;
		}
	}
	if (mBaseLayer->CreateTexture(4))
	{
		wti.gltexture = mBaseLayer->gltexture[4];
		return &wti;
	}
	return NULL;
}

//===========================================================================
// 
//
//
//===========================================================================

const PatchTextureInfo *FMaterial::GetPatchTextureInfo()
{
	if (mBaseLayer->CreatePatch())
	{
		pti.glpatch = mBaseLayer->glpatch;
		return &pti;
	}
	return NULL;
}

//===========================================================================
//
// This function is needed here to temporarily manipulate the texture
// for per-wall scaling so that the coordinate functions return proper
// results. Doing this here is much easier than having the calling code
// make these calculations.
//
//===========================================================================

void FMaterial::SetWallScaling(fixed_t x, fixed_t y)
{
	if (x != tempScaleX)
	{
		fixed_t scale_x = FixedMul(x, tex->xScale);
		int foo = (Width[GLUSE_TEXTURE] << 17) / scale_x; 
		RenderWidth[GLUSE_TEXTURE] = (foo >> 1) + (foo & 1); 
		wti.scalex = scale_x/(float)FRACUNIT;
		tempScaleX = x;
	}
	if (y != tempScaleY)
	{
		fixed_t scale_y = FixedMul(y, tex->yScale);
		int foo = (Height[GLUSE_TEXTURE] << 17) / scale_y; 
		RenderHeight[GLUSE_TEXTURE] = (foo >> 1) + (foo & 1); 
		wti.scaley = scale_y/(float)FRACUNIT;
		tempScaleY = y;
	}
}

//===========================================================================
//
//
//
//===========================================================================

fixed_t FMaterial::RowOffset(fixed_t rowoffset) const
{
	if (tempScaleX == FRACUNIT)
	{
		if (wti.scaley==1.f || tex->bWorldPanning) return rowoffset;
		else return quickertoint(rowoffset/wti.scaley);
	}
	else
	{
		if (tex->bWorldPanning) return FixedDiv(rowoffset, tempScaleY);
		else return quickertoint(rowoffset/wti.scaley);
	}
}

//===========================================================================
//
//
//
//===========================================================================

float FMaterial::RowOffset(float rowoffset) const
{
	if (tempScaleX == FRACUNIT)
	{
		if (wti.scaley==1.f || tex->bWorldPanning) return rowoffset;
		else return rowoffset / wti.scaley;
	}
	else
	{
		if (tex->bWorldPanning) return rowoffset / TO_GL(tempScaleY);
		else return rowoffset / wti.scaley;
	}
}

//===========================================================================
//
//
//
//===========================================================================

fixed_t FMaterial::TextureOffset(fixed_t textureoffset) const
{
	if (tempScaleX == FRACUNIT)
	{
		if (wti.scalex==1.f || tex->bWorldPanning) return textureoffset;
		else return quickertoint(textureoffset/wti.scalex);
	}
	else
	{
		if (tex->bWorldPanning) return FixedDiv(textureoffset, tempScaleX);
		else return quickertoint(textureoffset/wti.scalex);
	}
}


//===========================================================================
//
//
//
//===========================================================================

float FMaterial::TextureOffset(float textureoffset) const
{
	if (tempScaleX == FRACUNIT)
	{
		if (wti.scalex==1.f || tex->bWorldPanning) return textureoffset;
		else return textureoffset/wti.scalex;
	}
	else
	{
		if (tex->bWorldPanning) return textureoffset / TO_GL(tempScaleX);
		else return textureoffset/wti.scalex;
	}
}


//===========================================================================
//
// Returns the size for which texture offset coordinates are used.
//
//===========================================================================

fixed_t FMaterial::TextureAdjustWidth(ETexUse i) const
{
	if (tex->bWorldPanning) 
	{
		if (i == GLUSE_PATCH || tempScaleX == FRACUNIT) return RenderWidth[i];
		else return FixedDiv(Width[i], tempScaleX);
	}
	else return Width[i];
}

//===========================================================================
//
//
//
//===========================================================================

void FMaterial::BindToFrameBuffer()
{
	if (mBaseLayer->gltexture == NULL)
	{
		// must create the hardware texture first
		mBaseLayer->Bind(0, CM_DEFAULT, 0, 0, false, false);
		FHardwareTexture::Unbind(0);
	}
	mBaseLayer->gltexture[0]->BindToFrameBuffer();
}

//===========================================================================
//
// GetRect
//
//===========================================================================

void FMaterial::GetRect(FloatRect * r, ETexUse i) const
{
	r->left=-(float)GetScaledLeftOffset(i);
	r->top=-(float)GetScaledTopOffset(i);
	r->width=(float)TextureWidth(i);
	r->height=(float)TextureHeight(i);
}


//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FTexture * tex)
{
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		FMaterial *gltex = tex->gl_info.Material;
		if (gltex == NULL) 
		{
			gltex = new FMaterial(tex, false);
		}
		return gltex;
	}
	return NULL;
}

FMaterial * FMaterial::ValidateTexture(FTextureID no, bool translate)
{
	return ValidateTexture(translate? TexMan(no) : TexMan[no]);
}


//==========================================================================
//
// Flushes all hardware dependent data
//
//==========================================================================

void FMaterial::FlushAll()
{
	for(int i=mMaterials.Size()-1;i>=0;i--)
	{
		mMaterials[i]->Clean(true);
	}
	// This is for shader layers. All shader layers must be managed by the texture manager
	// so this will catch everything.
	for(int i=TexMan.NumTextures()-1;i>=0;i--)
	{
		FGLTexture *gltex = TexMan.ByIndex(i)->gl_info.SystemTexture;
		if (gltex != NULL) gltex->Clean(true);
	}
}

//==========================================================================
//
// Deletes all hardware dependent data
//
//==========================================================================

void FMaterial::DeleteAll()
{
	for(int i=mMaterials.Size()-1;i>=0;i--)
	{
		mMaterials[i]->tex->gl_info.Material = NULL;
		delete mMaterials[i];
	}
	mMaterials.Clear();
	mMaterials.ShrinkToFit();
}

//==========================================================================
//
// Prints some texture info
//
//==========================================================================

int FGLTexture::Dump(int i)
{
	int cnt = 0;
	int lump = tex->GetSourceLump();
	Printf(PRINT_LOG, "Texture '%s' (Index %d, Lump %d, Name '%s'):\n", tex->Name, i, lump, Wads.GetLumpFullName(lump));
	if (hirestexture) Printf(PRINT_LOG, "\tHirestexture\n");
	if (glpatch) Printf(PRINT_LOG, "\tPatch\n"),cnt++;
	if (gltexture[0]) Printf(PRINT_LOG, "\tTexture (x:no,  y:no )\n"),cnt++;
	if (gltexture[1]) Printf(PRINT_LOG, "\tTexture (x:yes, y:no )\n"),cnt++;
	if (gltexture[2]) Printf(PRINT_LOG, "\tTexture (x:no,  y:yes)\n"),cnt++;
	if (gltexture[3]) Printf(PRINT_LOG, "\tTexture (x:yes, y:yes)\n"),cnt++;
	if (gltexture[4]) Printf(PRINT_LOG, "\tTexture precache\n"),cnt++;
	return cnt;
}

CCMD(textureinfo)
{
	int cnth = 0, cntt = 0, pix = 0;
	for(int i=0; i<TexMan.NumTextures(); i++)
	{
		FTexture *tex = TexMan.ByIndex(i);
		FGLTexture *systex = tex->gl_info.SystemTexture;
		if (systex != NULL) 
		{
			int cnt = systex->Dump(i);
			cnth+=cnt;
			cntt++;
			pix += cnt * tex->GetWidth() * tex->GetHeight();
		}
	}
	Printf(PRINT_LOG, "%d system textures, %d hardware textures, %d pixels\n", cntt, cnth, pix);
}