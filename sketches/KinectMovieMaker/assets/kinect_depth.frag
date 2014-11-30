#version 150

// USER TEXTURES:

uniform sampler2D	uTextureColor;
uniform sampler2D	uTextureDepth;
uniform sampler2D	uTextureLookup;
uniform sampler2D   uTextureBody;

// SHADER VARS:

in vec2				vTexCoord0;
out vec4 			fragColor;

void main( void )
{
	// Get body alpha mask:
	float tBodyMask = 1.0 - texture( uTextureBody, vTexCoord0 ).r;
	// Get depth-to-color lookup coordinate:
	vec2 tCoordAdj = texture( uTextureLookup, vTexCoord0 ).rg;
	// Get masked-user color pixel:
	vec4 tUserColor = vec4( texture( uTextureColor, tCoordAdj ).rgb, tBodyMask );

	// todo temp test:
	float tBri = ( tUserColor.r + tUserColor.g + tUserColor.b ) / 3.0;
	tUserColor.r = tBri; 
	tUserColor.g = tBri; 
	tUserColor.b = tBri; 

	// Set final color:
	fragColor = tUserColor;


	// todo debug testing
	//fragColor = texture( uTextureBody, vTexCoord0 );
}
 