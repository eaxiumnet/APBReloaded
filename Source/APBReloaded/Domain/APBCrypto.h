#pragma once
// APBCrypto.h — header-only, pure C++17 cryptographic primitives
// SHA-256 (FIPS 180-4), HMAC-SHA256 (RFC 2104), PBKDF2-HMAC-SHA256 (RFC 2898)
// No platform headers. No external libraries.
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <random>

namespace apb {
namespace detail {

static constexpr uint32_t K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,
    0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
    0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,
    0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,
    0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
    0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,
    0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,
    0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
    0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static constexpr uint32_t H0_256[8] = {
    0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
    0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
};

inline uint32_t rotr32(uint32_t x,int n){return(x>>n)|(x<<(32-n));}
inline uint32_t ch256(uint32_t x,uint32_t y,uint32_t z){return(x&y)^(~x&z);}
inline uint32_t maj256(uint32_t x,uint32_t y,uint32_t z){return(x&y)^(x&z)^(y&z);}
inline uint32_t sig0(uint32_t x){return rotr32(x,2)^rotr32(x,13)^rotr32(x,22);}
inline uint32_t sig1(uint32_t x){return rotr32(x,6)^rotr32(x,11)^rotr32(x,25);}
inline uint32_t gam0(uint32_t x){return rotr32(x,7)^rotr32(x,18)^(x>>3);}
inline uint32_t gam1(uint32_t x){return rotr32(x,17)^rotr32(x,19)^(x>>10);}

inline void sha256_block(uint32_t st[8],const uint8_t blk[64]){
    uint32_t w[64];
    for(int i=0;i<16;++i)
        w[i]=(uint32_t(blk[4*i])<<24)|(uint32_t(blk[4*i+1])<<16)
            |(uint32_t(blk[4*i+2])<<8)|uint32_t(blk[4*i+3]);
    for(int i=16;i<64;++i)
        w[i]=gam1(w[i-2])+w[i-7]+gam0(w[i-15])+w[i-16];
    uint32_t a=st[0],b=st[1],c=st[2],d=st[3],
             e=st[4],f=st[5],g=st[6],h=st[7];
    for(int i=0;i<64;++i){
        uint32_t t1=h+sig1(e)+ch256(e,f,g)+K256[i]+w[i];
        uint32_t t2=sig0(a)+maj256(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    st[0]+=a;st[1]+=b;st[2]+=c;st[3]+=d;
    st[4]+=e;st[5]+=f;st[6]+=g;st[7]+=h;
}

inline std::array<uint8_t,32> sha256_raw(const uint8_t* data,size_t len){
    uint32_t st[8];
    for(int i=0;i<8;++i) st[i]=H0_256[i];
    uint64_t bit_len=uint64_t(len)*8;
    size_t padlen=(len%64<56)?56-len%64:120-len%64;
    size_t total=len+padlen+8;
    std::vector<uint8_t> msg(total,0);
    std::memcpy(msg.data(),data,len);
    msg[len]=0x80u;
    for(int i=0;i<8;++i) msg[total-8+i]=uint8_t(bit_len>>(56-8*i));
    for(size_t off=0;off<total;off+=64) sha256_block(st,msg.data()+off);
    std::array<uint8_t,32> out;
    for(int i=0;i<8;++i){
        out[4*i]=uint8_t(st[i]>>24);out[4*i+1]=uint8_t(st[i]>>16);
        out[4*i+2]=uint8_t(st[i]>>8);out[4*i+3]=uint8_t(st[i]);
    }
    return out;
}

} // namespace detail

// ── public API ────────────────────────────────────────────────────────────────

inline std::array<uint8_t,32> sha256(const uint8_t* data,size_t len){
    return detail::sha256_raw(data,len);
}

inline std::array<uint8_t,32> hmac_sha256(
    const uint8_t* key,size_t klen,
    const uint8_t* data,size_t dlen)
{
    uint8_t k[64]={};
    if(klen>64){auto h=detail::sha256_raw(key,klen);std::memcpy(k,h.data(),32);}
    else{std::memcpy(k,key,klen);}
    uint8_t ip[64],op[64];
    for(int i=0;i<64;++i){ip[i]=k[i]^0x36u;op[i]=k[i]^0x5cu;}
    std::vector<uint8_t> inn(64+dlen);
    std::memcpy(inn.data(),ip,64);
    std::memcpy(inn.data()+64,data,dlen);
    auto hi=detail::sha256_raw(inn.data(),inn.size());
    uint8_t out_buf[96];
    std::memcpy(out_buf,op,64);
    std::memcpy(out_buf+64,hi.data(),32);
    return detail::sha256_raw(out_buf,96);
}

inline std::vector<uint8_t> pbkdf2_hmac_sha256(
    const uint8_t* pass,size_t plen,
    const uint8_t* salt,size_t slen,
    uint32_t iters=10000,uint32_t dklen=32)
{
    std::vector<uint8_t> dk;
    for(uint32_t blk=1;dk.size()<dklen;++blk){
        std::vector<uint8_t> s(slen+4);
        std::memcpy(s.data(),salt,slen);
        s[slen]=uint8_t(blk>>24);s[slen+1]=uint8_t(blk>>16);
        s[slen+2]=uint8_t(blk>>8);s[slen+3]=uint8_t(blk);
        auto u=hmac_sha256(pass,plen,s.data(),s.size());
        auto t=u;
        for(uint32_t j=1;j<iters;++j){
            u=hmac_sha256(pass,plen,u.data(),32);
            for(int k=0;k<32;++k) t[k]^=u[k];
        }
        for(auto b:t) dk.push_back(b);
    }
    dk.resize(dklen);
    return dk;
}

inline std::string hex_encode(const uint8_t* data,size_t len){
    static constexpr char h[]="0123456789abcdef";
    std::string out;out.reserve(len*2);
    for(size_t i=0;i<len;++i){out+=h[data[i]>>4];out+=h[data[i]&0xf];}
    return out;
}

inline bool is_valid_secret_material(const std::string& material){
    if(material.size()<64 || (material.size()%2)!=0) return false;
    for(char c:material){
        const bool digit=c>='0'&&c<='9';
        const bool lower=c>='a'&&c<='f';
        const bool upper=c>='A'&&c<='F';
        if(!digit&&!lower&&!upper) return false;
    }
    return true;
}

inline std::vector<uint8_t> hex_decode(const std::string& s){
    if (s.size() % 2 != 0) throw std::invalid_argument("odd length hex");
    auto v=[](char c)->uint8_t{
        if(c>='0'&&c<='9') return uint8_t(c-'0');
        if(c>='a'&&c<='f') return uint8_t(c-'a'+10);
        if(c>='A'&&c<='F') return uint8_t(c-'A'+10);
        throw std::invalid_argument("invalid hex character");
    };
    std::vector<uint8_t> out;out.reserve(s.size()/2);
    for(size_t i=0;i+1<s.size();i+=2) out.push_back((v(s[i])<<4)|v(s[i+1]));
    return out;
}

inline std::string hmac_sha256_hex(
    const uint8_t* key,size_t klen,
    const uint8_t* data,size_t dlen)
{
    auto mac=hmac_sha256(key,klen,data,dlen);
    return hex_encode(mac.data(),32);
}

// M16: replace body with BCryptGenRandom for CSPRNG on Windows
inline std::string random_hex(size_t n_bytes){
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::vector<uint8_t> buf(n_bytes);
    for(size_t i=0;i<n_bytes;){
        uint64_t v=gen();
        for(int b=0;b<8&&i<n_bytes;++b,++i) buf[i]=uint8_t(v>>(8*b));
    }
    return hex_encode(buf.data(),n_bytes);
}

} // namespace apb
