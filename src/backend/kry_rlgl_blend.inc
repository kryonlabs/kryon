/* Kryon-private capture hooks. Keep the renderer cache and actual GL state
 * together, including custom factors configured but not activated yet.
 * No declarations are added to the public rlgl/Kryon API. */
int kryon_rl_framebuffer_save(void)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    int framebuffer = 0;
    rlDrawRenderBatch(RLGL.currentBatch);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);
    return framebuffer;
#else
    return 0;
#endif
}

void kryon_rl_framebuffer_restore(int framebuffer)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    glBindFramebuffer(GL_FRAMEBUFFER,(unsigned int)framebuffer);
#else
    (void)framebuffer;
#endif
}

void kryon_rl_blend_capture(void)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    rlSetBlendFactorsSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,
                             GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_FUNC_ADD,GL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
#endif
}

void kryon_rl_blend_save(int *s)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    s[0] = RLGL.State.currentBlendMode;
    s[1] = RLGL.State.glBlendSrcFactor;
    s[2] = RLGL.State.glBlendDstFactor;
    s[3] = RLGL.State.glBlendEquation;
    s[4] = RLGL.State.glBlendSrcFactorRGB;
    s[5] = RLGL.State.glBlendDestFactorRGB;
    s[6] = RLGL.State.glBlendSrcFactorAlpha;
    s[7] = RLGL.State.glBlendDestFactorAlpha;
    s[8] = RLGL.State.glBlendEquationRGB;
    s[9] = RLGL.State.glBlendEquationAlpha;
    s[10] = RLGL.State.glCustomBlendModeModified;
    glGetIntegerv(GL_BLEND_SRC_RGB, &s[11]);
    glGetIntegerv(GL_BLEND_DST_RGB, &s[12]);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &s[13]);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &s[14]);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &s[15]);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &s[16]);
#else
    (void)s;
#endif
}

void kryon_rl_blend_restore(const int *s)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    rlDrawRenderBatch(RLGL.currentBatch);
    RLGL.State.currentBlendMode = s[0];
    RLGL.State.glBlendSrcFactor = s[1];
    RLGL.State.glBlendDstFactor = s[2];
    RLGL.State.glBlendEquation = s[3];
    RLGL.State.glBlendSrcFactorRGB = s[4];
    RLGL.State.glBlendDestFactorRGB = s[5];
    RLGL.State.glBlendSrcFactorAlpha = s[6];
    RLGL.State.glBlendDestFactorAlpha = s[7];
    RLGL.State.glBlendEquationRGB = s[8];
    RLGL.State.glBlendEquationAlpha = s[9];
    RLGL.State.glCustomBlendModeModified = s[10];
    glBlendFuncSeparate(s[11],s[12],s[13],s[14]);
    glBlendEquationSeparate(s[15],s[16]);
#else
    (void)s;
#endif
}
