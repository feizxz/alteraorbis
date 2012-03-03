static const char* fixedpipe_vert = {
    "uniform mat4 u_mvpMatrix;		// model-view-projection\n"
    "#if COLOR_MULTIPLIER == 1\n"
    "	uniform vec4 u_colorMult;		\n"
    "#endif\n"
    "attribute vec3 a_pos;			// vertex position\n"
    "#if COLORS == 1\n"
    "	attribute vec3 a_color;			// vertex color\n"
    "#endif\n"
    "#if TEXTURE0 == 1\n"
    "	#if TEXTURE0_TRANSFORM == 1\n"
    "		uniform mat4 u_texMat0;\n"
    "	#endif	\n"
    "	#if TEXTURE0_3COMP == 1\n"
    "		attribute vec3 a_uv0;		// note this is vec3 when there is a transform\n"
    "	#else\n"
    "		attribute vec2 a_uv0;\n"
    "	#endif\n"
    "	varying vec2 v_uv0;\n"
    "#endif\n"
    "#if TEXTURE1 == 1\n"
    "	#if TEXTURE1_TRANSFORM == 1\n"
    "		uniform mat4 u_texMat1;\n"
    "	#endif\n"
    "	#if TEXTURE1_3COMP == 1\n"
    "		attribute vec3 a_uv1;\n"
    "	#else\n"
    "		attribute vec2 a_uv1;\n"
    "	#endif\n"
    "	varying vec2 v_uv1;\n"
    "#endif\n"
    "#if LIGHTING_DIFFUSE == 1\n"
    "	uniform mat4 u_normalMatrix;	// normal transformation\n"
    "	uniform vec3 u_lightDir;		// light direction, eye space (x,y,z,0)\n"
    "	uniform vec4 u_ambient;			// ambient light. ambient+diffuse = 1\n"
    "	uniform vec4 u_diffuse;			// diffuse light\n"
    "	attribute vec3 a_normal;		// vertex normal\n"
    "#endif\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "	#if COLOR_MULTIPLIER == 0\n"
    "		vec4 color = vec4( 1,1,1,1 );\n"
    "	#elif COLOR_MULTIPLIER == 1\n"
    "		vec4 color = u_colorMult;\n"
    "	#endif\n"
    "	\n"
    "	#if COLORS == 1\n"
    "		color *= a_color;\n"
    "	#endif\n"
    "	#if LIGHTING_DIFFUSE == 1\n"
    "		vec3 normal = normalize( ( u_normalMatrix * vec4( a_normal.x, a_normal.y, a_normal.z, 0 ) ).xyz );\n"
    "		float nDotL = max( dot( normal, u_lightDir ), 0.0 );\n"
    "		vec4 light = u_ambient + u_diffuse * nDotL;\n"
    "		\n"
    "		color *= light;\n"
    "	#endif\n"
    "	\n"
    "	#if TEXTURE0 == 1\n"
    "		#if TEXTURE0_TRANSFORM == 1\n"
    "			#if TEXTURE0_3COMP == 1\n"
    "				v_uv0 = ( u_texMat0 * vec4( a_uv0.x, a_uv0.y, a_uv0.z, 1 ) ).xy;\n"
    "			#else\n"
    "				v_uv0 = ( u_texMat0 * vec4( a_uv0.x, a_uv0.y, 0, 1 ) ).xy;\n"
    "			#endif\n"
    "		#else\n"
    "			v_uv0 = a_uv0;\n"
    "		#endif\n"
    "	#endif\n"
    "	#if TEXTURE1 == 1\n"
    "		#if TEXTURE1_TRANSFORM == 1\n"
    "			#if TEXTURE1_3COMP\n"
    "				v_uv1 = ( u_texMat1 * vec4( a_uv1.x, a_uv1.y, a_uv1.z, 1 ) ).xy;\n"
    "			#else\n"
    "				v_uv1 = ( u_texMat1 * vec4( a_uv1.x, a_uv1.y, 0, 1 ) ).xy;\n"
    "			#endif\n"
    "		#else\n"
    "			v_uv1 = a_uv1;\n"
    "		#endif\n"
    "	#endif\n"
    "	v_color = color;\n"
    "	gl_Position = u_mvpMatrix * vec4( a_pos.x, a_pos.y, a_pos.z, 1.0 );\n"
    "}\n"
};
static const char* fixedpipe_frag = {
    "#if TEXTURE0 == 1\n"
    "	uniform sampler2D texture0;\n"
    "	varying vec2 v_uv0;\n"
    "#endif\n"
    "#if TEXTURE1 == 1\n"
    "	uniform sampler2D texture1;\n"
    "	varying vec2 v_uv1;\n"
    "#endif\n"
    "varying vec4 v_color;\n"
    "void main() \n"
    "{\n"
    "	vec4 color = v_color;\n"
    "	#if TEXTURE0 == 1\n"
    "		#if TEXTURE0_ALPHA_ONLY == 1\n"
    "			color.a *= texture2D( texture0, v_uv0).a;\n"
    "		#else \n"
    "			color *= texture2D( texture0, v_uv0 );\n"
    "		#endif\n"
    "	#endif\n"
    "	#if TEXTURE1 == 1\n"
    "		#if TEXTURE1_ALPHA_ONLY == 1\n"
    "			color.a *= texture2D( texture1, v_uv1).a;\n"
    "		#else\n"
    "			color *= texture2D( texture1, v_uv1 );\n"
    "		#endif\n"
    "	#endif\n"
    "	gl_FragColor = color;\n"
    "}\n"
};