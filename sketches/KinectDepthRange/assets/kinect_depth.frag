#version 150

uniform sampler2D	uTextureColor;
uniform sampler2D	uTextureDepth;
uniform sampler2D	uTextureLookup;
uniform sampler2D   uTextureBody;

in vec2				vTexCoord0;
out vec4 			fragColor;

void main( void )
{
	// Get body alpha mask:
	float tBodyMask = texture( uTextureBody, vTexCoord0 ).a;
	// Get depth-to-color lookup coordinate:
	vec2 tCoordAdj = texture( uTextureLookup, vTexCoord0 ).rg;
	// Get masked-user color pixel:
	vec4 tUserColor = vec4( texture( uTextureColor, tCoordAdj ).rgb, tBodyMask );
	// Set final color:
	fragColor = tUserColor;
}
 