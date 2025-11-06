#version 410 core
in vec3 vDir;
out vec4 fragColor;
uniform samplerCube uSky;
void main(){
    fragColor = texture(uSky, normalize(vDir));
}
