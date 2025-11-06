#version 410 core
layout(location=0) in vec3 aPos;
out vec3 vDir;
uniform mat4 uProj;
uniform mat4 uView; // rotation only
void main(){
    vDir = aPos;
    vec4 p = uProj * uView * vec4(aPos, 1.0);
    gl_Position = p.xyww; // depth = 1
}
