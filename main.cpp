// =============================================================
//  MUHTESEM TANK vs TANK - SAVAS MEYDANI SURUMU (SFML 3 - ENDLESS)
//  V5 - GORSEL GELISTIRME SURUMU (Paletler, Partikuller, Arkaplan)
// =============================================================

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <iostream>

// =============================================================
// SABITLER
// =============================================================
const float PI = 3.14159265f;
const float PENCERE_W = 1920.f;
const float PENCERE_H = 1080.f;
const float ZEMIN_Y = 930.f;
const float TANK_BOLGE_MAX_X = PENCERE_W * 0.40f;
const float ADAM_BOLGE_MIN_X = PENCERE_W * 0.58f;

enum class Sahip { TANK, DUSMAN };
enum class SilahTipi { TOP, TABANCA, KELES, NISANCI, ROKET };
enum class GucTipi { CAN, TABANCA, KELES, NISANCI, ROKET };
enum class OyunDurumu { MENU, OYUN_ICI, DURAKLATILDI, OYUN_BITTI };
enum class PartikulTipi { ATES, DUMAN, KIVILCIM };

// =============================================================
// AYARLAR VE GLOBAL SISTEMLER
// =============================================================
float globalSes = 50.f;
float globalParlaklik = 255.f; // 0 (karanlik) - 255 (tam parlaklik)
OyunDurumu aktifDurum = OyunDurumu::MENU;

class OyunNesnesi;
std::vector<OyunNesnesi*> g_nesnelerEklenecek;

class SesYoneticisi {
public:
    sf::SoundBuffer bShoot, bExp, bHit, bSel;
    std::optional<sf::Sound> sShoot, sExp, sHit, sSel;

    SesYoneticisi() {
        if(bShoot.loadFromFile("assets/shoot.wav")) sShoot.emplace(bShoot);
        if(bExp.loadFromFile("assets/explosion.wav")) sExp.emplace(bExp);
        if(bHit.loadFromFile("assets/hit.wav")) sHit.emplace(bHit);
        if(bSel.loadFromFile("assets/select.wav")) sSel.emplace(bSel);
        guncelleSesSeviyesi();
    }

    void guncelleSesSeviyesi() {
        if(sShoot) sShoot->setVolume(globalSes);
        if(sExp) sExp->setVolume(globalSes);
        if(sHit) sHit->setVolume(globalSes);
        if(sSel) sSel->setVolume(globalSes);
    }

    void oynaShoot() { if(sShoot) sShoot->play(); }
    void oynaExp() { if(sExp) sExp->play(); }
    void oynaHit() { if(sHit) sHit->play(); }
    void oynaSel() { if(sSel) sSel->play(); }
};
SesYoneticisi* sesSys = nullptr;

// =============================================================
// SABLONLAR (TEMPLATES)
// =============================================================
template<typename T>
void nesneleriTemizle(std::vector<T*>& nesneler) {
    auto it = std::remove_if(nesneler.begin(), nesneler.end(), [](T* nesne) {
        if (nesne->silinecekMi()) {
            delete nesne;
            return true;
        }
        return false;
    });
    nesneler.erase(it, nesneler.end());
}

template<typename T1, typename T2>
bool carpistiMi(T1* n1, T2* n2) {
    auto s1 = n1->getSinirlar();
    auto s2 = n2->getSinirlar();
    if(s1.has_value() && s2.has_value()) {
        return s1->findIntersection(s2.value()).has_value();
    }
    return false;
}

// =============================================================
// TEMEL SINIFLAR
// =============================================================
class OyunNesnesi { //en temel abstract sinif, tum nesneler bundan tureyecek
public:
    virtual ~OyunNesnesi() = default;
    virtual void guncelle() = 0;
    virtual void ciz(sf::RenderWindow& pencere) = 0;
    virtual std::optional<sf::FloatRect> getSinirlar() { return std::nullopt; }
    virtual bool silinecekMi() { return false; }
    virtual void silinmeyeIsaretle() {}
    virtual bool isMermi() { return false; }
    virtual Sahip getSahip() { return Sahip::TANK; }
};

class Karakter : public OyunNesnesi {
protected:
    int maxCan;
    int can;
    sf::RectangleShape hitbox;
    int hasarSuresi = 0;
public:
    Karakter(int can_degeri) : maxCan(can_degeri), can(can_degeri) {}
    
    virtual void hareketEttir(float dx, float dy) = 0;
    virtual OyunNesnesi* atesEt() = 0;
    
    virtual void hasarAl() {
        if (can <= 0) return;
        can--;
        hasarSuresi = 20;
        if(sesSys) sesSys->oynaHit();
    }
    
    virtual void canEkle() {
        if (can < maxCan) can += 2;
        if(can > maxCan) can = maxCan;
    }
    
    int getCan() const { return can; }
    bool silinecekMi() override { return can <= 0; }
    std::optional<sf::FloatRect> getSinirlar() override { return hitbox.getGlobalBounds(); }
};

// =============================================================
// PARTIKUL
// =============================================================
class Partikul : public OyunNesnesi {
private:
    sf::CircleShape sekil;
    float hizX = 0.f;
    float hizY = 0.f;
    int omur = 0;
    int maxOmur = 0;
    PartikulTipi tip;

public:
    Partikul(float x, float y, PartikulTipi t, float sacilma = 10.f) : tip(t) {
        sekil.setPosition({x, y});
        
        if (tip == PartikulTipi::ATES) {
            sekil.setRadius(4.f + static_cast<float>(std::rand() % 6));
            hizX = (static_cast<float>(std::rand() % static_cast<int>(sacilma * 2.f)) - sacilma) * 0.9f;
            hizY = (static_cast<float>(std::rand() % static_cast<int>(sacilma * 2.f)) - sacilma) * 0.9f;
            maxOmur = 15 + (std::rand() % 15);
            sekil.setFillColor((std::rand()%2==0) ? sf::Color(255, 150, 0) : sf::Color(255, 50, 0));
        } 
        else if (tip == PartikulTipi::DUMAN) {
            sekil.setRadius(8.f + static_cast<float>(std::rand() % 10));
            hizX = (static_cast<float>(std::rand() % static_cast<int>(sacilma * 2.f)) - sacilma) * 0.3f;
            hizY = -1.f - static_cast<float>(std::rand() % 15) / 10.f; // Yukari dogru
            maxOmur = 40 + (std::rand() % 30);
            int gri = 40 + std::rand() % 60;
            sekil.setFillColor(sf::Color(gri, gri, gri, 180));
        } 
        else if (tip == PartikulTipi::KIVILCIM) {
            sekil.setRadius(2.f + static_cast<float>(std::rand() % 3));
            hizX = (static_cast<float>(std::rand() % static_cast<int>(sacilma * 2.f)) - sacilma) * 1.5f;
            hizY = (static_cast<float>(std::rand() % static_cast<int>(sacilma * 2.f)) - sacilma) * 1.5f;
            maxOmur = 20 + (std::rand() % 20);
            sekil.setFillColor(sf::Color(255, 255, 100));
        }
        sekil.setOrigin({sekil.getRadius(), sekil.getRadius()});
        omur = maxOmur;
    }

    void guncelle() override {
        sekil.move({hizX, hizY});
        if (tip == PartikulTipi::DUMAN) {
            hizY -= 0.02f; // Daha da hizli yuksel
            hizX *= 0.98f;
            sekil.setRadius(sekil.getRadius() + 0.15f); // Yavasca genisle
            sekil.setOrigin({sekil.getRadius(), sekil.getRadius()});
        } else {
            hizY += 0.25f; // Yercekimi
            hizX *= 0.95f;
        }
        
        omur--;
        sf::Color renk = sekil.getFillColor();
        renk.a = static_cast<std::uint8_t>((255.f * static_cast<float>(omur)) / static_cast<float>(maxOmur));
        sekil.setFillColor(renk);
    }

    void ciz(sf::RenderWindow& pencere) override { pencere.draw(sekil); }
    bool silinecekMi() override { return omur <= 0; }
};

void patlamaEfekti(float x, float y) {
    if(sesSys) sesSys->oynaExp();
    // Kivilcimlar
    for (int i = 0; i < 15; ++i) g_nesnelerEklenecek.push_back(new Partikul(x, y, PartikulTipi::KIVILCIM, 15.f));
    // Ates
    for (int i = 0; i < 20; ++i) g_nesnelerEklenecek.push_back(new Partikul(x, y, PartikulTipi::ATES, 10.f));
    // Duman
    for (int i = 0; i < 15; ++i) g_nesnelerEklenecek.push_back(new Partikul(x, y, PartikulTipi::DUMAN, 12.f));
}

// =============================================================
// MERMI VE ROKET
// =============================================================
class Mermi : public OyunNesnesi {
private:
    sf::ConvexShape aerodinamikMermi;
    sf::RectangleShape lazerMermi;
    bool lazerMi = false;
    float hizX = 0.f;
    float hizY = 0.f;
    float yercekimi = 0.22f;
    bool imha = false;
    Sahip kimeAit;
    sf::Clock dumanSaati;

public:
    Mermi(float x, float y, float aciDerece, SilahTipi tip, Sahip sahip) : kimeAit(sahip) {
        float guc = 22.f;
        sf::Color renk = sf::Color::Black;

        if (tip == SilahTipi::TOP) {
            guc = 25.f; renk = sf::Color(200, 150, 50);
        } else if (tip == SilahTipi::TABANCA) {
            guc = 27.f; renk = sf::Color(230, 210, 90);
        } else if (tip == SilahTipi::KELES) {
            guc = 31.f; renk = sf::Color(255, 130, 25);
        } else if (tip == SilahTipi::NISANCI) {
            lazerMi = true; guc = 45.f; renk = sf::Color(50, 255, 255); yercekimi = 0.05f;
        }

        if (lazerMi) {
            lazerMermi.setSize({30.f, 4.f}); lazerMermi.setOrigin({15.f, 2.f});
            lazerMermi.setPosition({x, y}); lazerMermi.setFillColor(renk);
            lazerMermi.setRotation(sf::degrees(aciDerece));
        } else {
            aerodinamikMermi.setPointCount(5);
            aerodinamikMermi.setPoint(0, {12.f, 0.f}); // Sivri uc
            aerodinamikMermi.setPoint(1, {0.f, 4.f});
            aerodinamikMermi.setPoint(2, {-8.f, 4.f});
            aerodinamikMermi.setPoint(3, {-8.f, -4.f});
            aerodinamikMermi.setPoint(4, {0.f, -4.f});

            aerodinamikMermi.setOrigin({2.f, 0.f});
            aerodinamikMermi.setPosition({x, y});
            aerodinamikMermi.setFillColor(renk);
            aerodinamikMermi.setOutlineThickness(1.f);
            aerodinamikMermi.setOutlineColor(sf::Color(50,50,50));
            aerodinamikMermi.setRotation(sf::degrees(aciDerece));
        }

        const float aciRadyan = aciDerece * PI / 180.f;
        hizX = guc * std::cos(aciRadyan); hizY = guc * std::sin(aciRadyan);
    }

    void guncelle() override {
        hizY += yercekimi;
        if (lazerMi) {
            lazerMermi.move({hizX, hizY});
        } else {
            aerodinamikMermi.move({hizX, hizY});
            float ucusAcisi = std::atan2(hizY, hizX) * 180.f / PI;
            aerodinamikMermi.setRotation(sf::degrees(ucusAcisi));
            
            // Arkasinda ince duman izi (Trail)
            if (dumanSaati.getElapsedTime().asSeconds() > 0.03f) {
                sf::Vector2f pos = aerodinamikMermi.getPosition();
                g_nesnelerEklenecek.push_back(new Partikul(pos.x, pos.y, PartikulTipi::DUMAN, 2.f));
                dumanSaati.restart();
            }
        }
    }

    void ciz(sf::RenderWindow& pencere) override {
        if (lazerMi) pencere.draw(lazerMermi);
        else pencere.draw(aerodinamikMermi);
    }
    std::optional<sf::FloatRect> getSinirlar() override {
        return lazerMi ? lazerMermi.getGlobalBounds() : aerodinamikMermi.getGlobalBounds();
    }
    void silinmeyeIsaretle() override { imha = true; }
    bool silinecekMi() override {
        const sf::Vector2f p = lazerMi ? lazerMermi.getPosition() : aerodinamikMermi.getPosition();
        return imha || p.y > PENCERE_H + 70.f || p.x < -70.f || p.x > PENCERE_W + 70.f || p.y > ZEMIN_Y + 20.f;
    }
    bool isMermi() override { return true; }
    Sahip getSahip() override { return kimeAit; }
};

// ... Baloncuk ve Bulut siniflari ayni kalacak (Kisitli yer yuzunden ozetlendi) ...
class Baloncuk : public OyunNesnesi {
    // ...
    sf::CircleShape disKure;
    GucTipi tip;
    float salinim = 0.f;
    float dusmeHizi = 0.f;
    bool imha = false;
public:
    Baloncuk(float x, float y, GucTipi t) : tip(t) {
        disKure.setRadius(24.f); disKure.setOrigin({24.f, 24.f}); disKure.setPosition({x, y});
        disKure.setFillColor(sf::Color(255, 255, 255, 160));
        disKure.setOutlineColor(t==GucTipi::CAN ? sf::Color(50,255,100) : sf::Color(255,180,50));
        disKure.setOutlineThickness(3.f);
    }
    void guncelle() override {
        salinim += 0.04f;
        dusmeHizi = 1.2f + std::sin(salinim * 2.f) * 0.4f;
        disKure.move({std::sin(salinim) * 2.5f, dusmeHizi});
        if (disKure.getPosition().y > ZEMIN_Y - 20.f) imha = true;
    }
    void ciz(sf::RenderWindow& pencere) override {
        const sf::Vector2f p = disKure.getPosition();
        // Parlayan hale
        sf::CircleShape hale(28.f); hale.setOrigin({28.f, 28.f}); hale.setPosition(p);
        hale.setFillColor(sf::Color(255, 255, 200, static_cast<std::uint8_t>(30 + 20 * std::sin(salinim * 3.f))));
        pencere.draw(hale);
        pencere.draw(disKure);

        if (tip == GucTipi::CAN) {
            // Kirmizi kalp + beyaz arti
            sf::CircleShape kalp(10.f); kalp.setOrigin({10.f, 10.f}); kalp.setPosition(p);
            kalp.setFillColor(sf::Color(255, 40, 60));
            sf::RectangleShape dikey({6.f, 16.f}); dikey.setOrigin({3.f, 8.f}); dikey.setPosition(p); dikey.setFillColor(sf::Color::White);
            sf::RectangleShape yatay({16.f, 6.f}); yatay.setOrigin({8.f, 3.f}); yatay.setPosition(p); yatay.setFillColor(sf::Color::White);
            pencere.draw(kalp); pencere.draw(dikey); pencere.draw(yatay);
        } else if (tip == GucTipi::TABANCA) {
            sf::RectangleShape govde({18.f, 8.f}); govde.setOrigin({9.f, 4.f}); govde.setPosition(p); govde.setFillColor(sf::Color(70, 70, 80));
            sf::RectangleShape sap({6.f, 11.f}); sap.setOrigin({3.f, 0.f}); sap.setPosition({p.x - 3.f, p.y + 4.f}); sap.setFillColor(sf::Color(110, 80, 50));
            pencere.draw(govde); pencere.draw(sap);
        } else if (tip == GucTipi::KELES) {
            sf::RectangleShape govde({28.f, 7.f}); govde.setOrigin({14.f, 3.5f}); govde.setPosition(p); govde.setFillColor(sf::Color(40, 40, 45));
            sf::RectangleShape sap({14.f, 6.f}); sap.setOrigin({7.f, 3.f}); sap.setPosition({p.x - 8.f, p.y + 1.f}); sap.setFillColor(sf::Color(140, 90, 50));
            sf::RectangleShape sarjor({5.f, 13.f}); sarjor.setOrigin({2.5f, 0.f}); sarjor.setPosition({p.x + 2.f, p.y + 5.f}); sarjor.setFillColor(sf::Color(60, 60, 60));
            pencere.draw(sap); pencere.draw(govde); pencere.draw(sarjor);
        } else if (tip == GucTipi::NISANCI) {
            sf::RectangleShape govde({38.f, 5.f}); govde.setOrigin({19.f, 2.5f}); govde.setPosition(p); govde.setFillColor(sf::Color(35, 35, 45));
            sf::CircleShape durbun(5.f); durbun.setOrigin({5.f, 5.f}); durbun.setPosition({p.x - 2.f, p.y - 7.f}); durbun.setFillColor(sf::Color(60, 60, 80));
            sf::CircleShape lens(2.5f); lens.setOrigin({2.5f, 2.5f}); lens.setPosition({p.x - 2.f, p.y - 7.f}); lens.setFillColor(sf::Color(100, 200, 255, 180));
            pencere.draw(govde); pencere.draw(durbun); pencere.draw(lens);
        } else if (tip == GucTipi::ROKET) {
            sf::RectangleShape govde({30.f, 11.f}); govde.setOrigin({15.f, 5.5f}); govde.setPosition(p); govde.setFillColor(sf::Color(55, 70, 55));
            sf::CircleShape agiz(5.5f); agiz.setOrigin({5.5f, 5.5f}); agiz.setPosition({p.x + 14.f, p.y}); agiz.setFillColor(sf::Color(220, 50, 30));
            sf::CircleShape egzoz(4.f); egzoz.setOrigin({4.f, 4.f}); egzoz.setPosition({p.x - 15.f, p.y}); egzoz.setFillColor(sf::Color(30, 30, 30));
            pencere.draw(govde); pencere.draw(agiz); pencere.draw(egzoz);
        }
    }
    std::optional<sf::FloatRect> getSinirlar() override { return disKure.getGlobalBounds(); }
    void silinmeyeIsaretle() override { imha = true; }
    bool silinecekMi() override { return imha; }
    GucTipi getTip() const { return tip; }
};

class Bulut : public OyunNesnesi {
private:
    std::vector<sf::CircleShape> puflar;
    float posX, posY, hizX;
    bool imha = false;
    sf::Clock birakmaSaati;
public:
    Bulut(float x, float y, float hiz) : posX(x), posY(y), hizX(hiz) {
        int adet = 5 + std::rand() % 4;
        for(int i = 0; i < adet; i++) {
            sf::CircleShape c(28.f + std::rand() % 22);
            c.setFillColor(sf::Color(240, 245, 255, 180));
            c.setPosition({x + (i * 28.f), y + (std::rand() % 20 - 10.f)});
            puflar.push_back(c);
        }
    }
    void guncelle() override {
        posX += hizX;
        for(auto& c : puflar) c.move({hizX, 0});
        if(posX < -400.f || posX > PENCERE_W + 400.f) imha = true;
    }
    void ciz(sf::RenderWindow& pencere) override {
        for(auto& c : puflar) pencere.draw(c);
    }
    bool silinecekMi() override { return imha; }
    Baloncuk* silahBirak() {
        if (birakmaSaati.getElapsedTime().asSeconds() > 3.0f) {
            if (std::rand() % 100 < 3) { 
                birakmaSaati.restart();
                return new Baloncuk(posX + 60.f, posY + 40.f, static_cast<GucTipi>(std::rand() % 5));
            }
        }
        return nullptr;
    }
};

// =============================================================
// TANK SINIFI (Karakter'den turer)
// =============================================================
class Tank : public Karakter {
protected:
    sf::RectangleShape govde, kule, namlu;
    sf::RectangleShape paletGovde;
    std::vector<sf::CircleShape> icTekerler;
    
    float namluAcisi = -14.f;
    float recoilOfset = 0.f;
    sf::Clock atisSaati;
    const float NAMLU_OFS_X = 66.f;
    const float NAMLU_OFS_Y = -15.f;

    void govdeRenginiAyarla(sf::Color renk) { govde.setFillColor(renk); }
    void kuleRenginiAyarla(sf::Color renk) { kule.setFillColor(renk); }
    void namluAcisiAyarla(float aci) { namluAcisi = aci; }
    void recoilGuncelle() {
        if (recoilOfset < 0.f) recoilOfset = std::min(0.f, recoilOfset + 0.6f);
    }
    void namluPozisyonuGuncelle() {
        const float rad = namluAcisi * PI / 180.f;
        namlu.setPosition({
            govde.getPosition().x + NAMLU_OFS_X + recoilOfset * std::cos(rad),
            govde.getPosition().y + NAMLU_OFS_Y + recoilOfset * std::sin(rad)
        });
        namlu.setRotation(sf::degrees(namluAcisi));
    }
    void hareketEttirTemel(float dx, float dy) {
        paletGovde.move({dx, dy});
        for(auto& t : icTekerler) t.move({dx, dy});
        govde.move({dx, dy}); kule.move({dx, dy}); hitbox.move({dx, dy});
    }
    OyunNesnesi* atesEtTemel(float bekleme, float aci, Sahip sahip) {
        if (atisSaati.getElapsedTime().asSeconds() < bekleme) return nullptr;
        atisSaati.restart();
        if(sesSys) sesSys->oynaShoot();
        recoilOfset = -12.f;
        const float rad = aci * PI / 180.f;
        const sf::Vector2f namluUcu = {
            govde.getPosition().x + NAMLU_OFS_X + namlu.getSize().x * std::cos(rad),
            govde.getPosition().y + NAMLU_OFS_Y + namlu.getSize().x * std::sin(rad)
        };
        // Namlu ucu atesi ve dumani
        g_nesnelerEklenecek.push_back(new Partikul(namluUcu.x, namluUcu.y, PartikulTipi::ATES, 8.f));
        g_nesnelerEklenecek.push_back(new Partikul(namluUcu.x, namluUcu.y, PartikulTipi::DUMAN, 5.f));
        
        return new Mermi(namluUcu.x, namluUcu.y, aci, SilahTipi::TOP, sahip);
    }
    

public:
    Tank(float x) : Karakter(10) {
        // Palet Govde
        paletGovde.setSize({140.f, 26.f});
        paletGovde.setPosition({x, ZEMIN_Y - 26.f});
        paletGovde.setFillColor(sf::Color::Red);
        paletGovde.setOutlineThickness(2.f);
        paletGovde.setOutlineColor(sf::Color(80, 80, 80));

        // Ic tekerler
        for(int i=0; i<5; i++) {
            sf::CircleShape t(9.f);
            t.setFillColor(sf::Color(45, 45, 45));
            t.setOutlineThickness(1.f);
            t.setOutlineColor(sf::Color(70, 70, 70));
            t.setPosition({x + 6.f + i*27.f, ZEMIN_Y - 22.f});
            icTekerler.push_back(t);
        }

        govde.setSize({130.f, 35.f}); govde.setPosition({x + 5.f, ZEMIN_Y - 61.f});
        govde.setFillColor(sf::Color(65, 80, 55)); // Kamuflaj yesili

        kule.setSize({60.f, 25.f}); kule.setPosition({x + 35.f, ZEMIN_Y - 86.f});
        kule.setFillColor(sf::Color(30, 30, 30)); // Kamuflaj koyu yesil

        namlu.setSize({80.f, 15.f}); namlu.setOrigin({0.f, 5.f});
        namlu.setFillColor(sf::Color(35, 35, 35));

        hitbox.setSize({140.f, 85.f}); hitbox.setPosition({x, ZEMIN_Y - 85.f});
    }

    void guncelle() override {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) hareketEttir(-6.f, 0.f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) hareketEttir(6.f, 0.f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            namluAcisi -= 0.60f; if (namluAcisi < -85.f) namluAcisi = -85.f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            namluAcisi += 0.60f; if (namluAcisi > 15.f) namluAcisi = 15.f;
        }
        recoilGuncelle();
        namluPozisyonuGuncelle();
        
        // Ic tekerleklerin donme efekti
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            for(auto& t : icTekerler) t.rotate(sf::degrees(-10.f));
            if(std::rand()%10 == 0) g_nesnelerEklenecek.push_back(new Partikul(paletGovde.getPosition().x + 140.f, ZEMIN_Y-5.f, PartikulTipi::DUMAN, 2.f)); // Toz
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            for(auto& t : icTekerler) t.rotate(sf::degrees(10.f));
            if(std::rand()%10 == 0) g_nesnelerEklenecek.push_back(new Partikul(paletGovde.getPosition().x, ZEMIN_Y-5.f, PartikulTipi::DUMAN, 2.f)); // Toz
        }

        if (hasarSuresi > 0) {
            hasarSuresi--;
            govdeRenginiAyarla(sf::Color(255, 100, 100));
        } else {
            govdeRenginiAyarla(sf::Color(65, 80, 55));
        }
    }

    void hareketEttir(float dx, float dy) override {
        if (hitbox.getPosition().x + dx < 10.f) return;
        if (hitbox.getPosition().x + hitbox.getSize().x + dx > TANK_BOLGE_MAX_X) return;
        hareketEttirTemel(dx, dy);
    }

    OyunNesnesi* atesEt() override {
        return atesEtTemel(0.5f, namluAcisi, Sahip::TANK);
    }

    void ciz(sf::RenderWindow& pencere) override {

    pencere.draw(paletGovde);

    for(auto& t : icTekerler) {

        pencere.draw(t);

        sf::CircleShape vida(2.f);

        vida.setFillColor(sf::Color(20,20,20));

        vida.setPosition({
            t.getPosition().x + 7.f,
            t.getPosition().y + 7.f
        });

        pencere.draw(vida);
    }

    pencere.draw(govde);
    pencere.draw(kule);
    pencere.draw(namlu);

    // ===== RADAR =====

    sf::CircleShape radar(20.f);// 25 inc radar eklendi

    radar.setOrigin({15.f, 15.f});

    radar.setPosition({
        govde.getPosition().x + 60.f,
        govde.getPosition().y - 50.f
    });

    radar.setFillColor(sf::Color(0,255,0,120));

    pencere.draw(radar);

    // ===== CAN BARI =====

    sf::RectangleShape hpBg({70.f, 8.f});

    hpBg.setPosition({
        govde.getPosition().x + 30.f,
        govde.getPosition().y - 15.f
    });

    hpBg.setFillColor(sf::Color(100, 0, 0, 220));

    sf::RectangleShape hpFg({
        70.f * (can / (float)maxCan),
        8.f
    });

    hpFg.setPosition(hpBg.getPosition());

    hpFg.setFillColor(sf::Color(50, 255, 50, 220));

    pencere.draw(hpBg);

    pencere.draw(hpFg);
}
};

class DusmanTank : public Tank {
private:
    int zorluk;
    sf::Clock zekaSaati;
    float hedefNamluAcisi = 195.f;
    float aiNamluAcisi = 195.f;

public:
    DusmanTank(float x, int zorlukSeviyesi) : Tank(x), zorluk(zorlukSeviyesi) {
        maxCan = 6 + zorluk; can = maxCan;
        govdeRenginiAyarla(sf::Color(120, 50, 45)); // Dusman tanki kirmizimsi kamuflaj
        kuleRenginiAyarla(sf::Color(100, 40, 35));
    }

    void guncelle() override {
        if (zekaSaati.getElapsedTime().asSeconds() > 1.2f) {
            hedefNamluAcisi = 160.f + static_cast<float>(std::rand() % 60);
            int hareketKarari = std::rand() % 100;
            if (hareketKarari < 30) hareketEttir(-5.f, 0.f);
            else if (hareketKarari < 60) hareketEttir(5.f, 0.f);
            zekaSaati.restart();
        }

        if (aiNamluAcisi < hedefNamluAcisi) aiNamluAcisi += 1.2f;
        if (aiNamluAcisi > hedefNamluAcisi) aiNamluAcisi -= 1.2f;

        namluAcisiAyarla(aiNamluAcisi);
        recoilGuncelle();
        namluPozisyonuGuncelle();

        if (hasarSuresi > 0) {
            hasarSuresi--; govdeRenginiAyarla(sf::Color(255, 100, 100));
        } else {
            govdeRenginiAyarla(sf::Color(120, 50, 45));
        }
    }

    void hareketEttir(float dx, float dy) override {
        if (hitbox.getPosition().x + dx < ADAM_BOLGE_MIN_X) return;
        if (hitbox.getPosition().x + hitbox.getSize().x + dx > PENCERE_W - 10.f) return;
        Tank::hareketEttirTemel(dx, dy);
        for(auto& t : icTekerler) t.rotate(sf::degrees(dx * 1.5f)); // Tekerlek donusu
    }

    OyunNesnesi* atesEt() override {
        float bekleme = 1.0f - (zorluk * 0.05f);
        if (bekleme < 0.3f) bekleme = 0.3f;
        return Tank::atesEtTemel(bekleme, aiNamluAcisi, Sahip::DUSMAN);
    }
    Sahip getSahip() override { return Sahip::DUSMAN; }
};

// =============================================================
// ARKA PLAN CIZIMI (GECE TEMASI - NIGHT)
// =============================================================
void savasMeydaniCiz(sf::RenderWindow& pencere) {
    // Gokyuzu Gradient - Gece koyu lacivert/siyah
    sf::VertexArray gokyuzu(sf::PrimitiveType::TriangleStrip, 6);
    gokyuzu[0].position = sf::Vector2f(0, 0);                     gokyuzu[0].color = sf::Color(5, 5, 20);
    gokyuzu[1].position = sf::Vector2f(PENCERE_W, 0);             gokyuzu[1].color = sf::Color(8, 8, 25);
    gokyuzu[2].position = sf::Vector2f(0, ZEMIN_Y * 0.45f);       gokyuzu[2].color = sf::Color(15, 15, 40);
    gokyuzu[3].position = sf::Vector2f(PENCERE_W, ZEMIN_Y * 0.45f); gokyuzu[3].color = sf::Color(20, 18, 50);
    gokyuzu[4].position = sf::Vector2f(0, ZEMIN_Y);               gokyuzu[4].color = sf::Color(25, 20, 45);
    gokyuzu[5].position = sf::Vector2f(PENCERE_W, ZEMIN_Y);       gokyuzu[5].color = sf::Color(30, 22, 50);
    pencere.draw(gokyuzu);

    // Yildizlar
    std::srand(42);
    for(int i = 0; i < 120; i++) {
        float sx = static_cast<float>(std::rand() % static_cast<int>(PENCERE_W));
        float sy = static_cast<float>(std::rand() % static_cast<int>(ZEMIN_Y * 0.85f));
        float sr = 1.f + (std::rand() % 3) * 0.5f;
        int parlaklik = 120 + std::rand() % 135;
        sf::CircleShape yildiz(sr);
        yildiz.setOrigin({sr, sr}); yildiz.setPosition({sx, sy});
        yildiz.setFillColor(sf::Color(parlaklik, parlaklik, parlaklik + 30, static_cast<std::uint8_t>(parlaklik)));
        pencere.draw(yildiz);
    }

    // Ay
    sf::CircleShape ay(45.f); ay.setOrigin({45.f, 45.f});
    ay.setPosition({1550.f, 120.f}); ay.setFillColor(sf::Color(220, 220, 200, 230));
    pencere.draw(ay);
    // Ay halesi
    sf::CircleShape ayHale(70.f); ayHale.setOrigin({70.f, 70.f});
    ayHale.setPosition({1550.f, 120.f}); ayHale.setFillColor(sf::Color(180, 180, 200, 30));
    pencere.draw(ayHale);
    // Ay krater golgeleri
    sf::CircleShape krater(6.f); krater.setOrigin({6.f, 6.f});
    krater.setPosition({1540.f, 110.f}); krater.setFillColor(sf::Color(190, 190, 175, 100));
    pencere.draw(krater);
    sf::CircleShape krater2(4.f); krater2.setOrigin({4.f, 4.f});
    krater2.setPosition({1560.f, 130.f}); krater2.setFillColor(sf::Color(190, 190, 175, 80));
    pencere.draw(krater2);

    // Dag siluetleri - gece koyu
    sf::VertexArray daglar(sf::PrimitiveType::TriangleStrip, 8);
    daglar[0].position = sf::Vector2f(0, ZEMIN_Y);               daglar[0].color = sf::Color(15, 15, 30, 220);
    daglar[1].position = sf::Vector2f(300, ZEMIN_Y - 300);       daglar[1].color = sf::Color(20, 20, 40, 200);
    daglar[2].position = sf::Vector2f(600, ZEMIN_Y);             daglar[2].color = sf::Color(15, 15, 30, 220);
    daglar[3].position = sf::Vector2f(900, ZEMIN_Y - 450);       daglar[3].color = sf::Color(25, 25, 50, 180);
    daglar[4].position = sf::Vector2f(1200, ZEMIN_Y);            daglar[4].color = sf::Color(15, 15, 30, 220);
    daglar[5].position = sf::Vector2f(1500, ZEMIN_Y - 250);      daglar[5].color = sf::Color(20, 20, 40, 190);
    daglar[6].position = sf::Vector2f(1920, ZEMIN_Y);            daglar[6].color = sf::Color(15, 15, 30, 220);
    daglar[7].position = sf::Vector2f(1920, ZEMIN_Y - 100);      daglar[7].color = sf::Color(18, 18, 35, 210);
    pencere.draw(daglar);

    // Arka Plan Binalar (Uzakta, soluk siluet)
    std::srand(200);
    for(int i = 0; i < 20; i++) {
        float bX = std::rand() % static_cast<int>(PENCERE_W);
        float bW = 50.f + std::rand() % 80;
        float bH = 150.f + std::rand() % 250;
        sf::RectangleShape bina({bW, bH});
        bina.setPosition({bX, ZEMIN_Y - bH});
        bina.setFillColor(sf::Color(20, 20, 35, 150));
        pencere.draw(bina);
    }

    // On Plan Binalar (detayli, gece isiklari)
    std::srand(100);
    for(int i = 0; i < 12; i++) {
        float bX = std::rand() % static_cast<int>(PENCERE_W);
        float bW = 80.f + std::rand() % 120;
        float bH = 200.f + std::rand() % 350;
        sf::RectangleShape bina({bW, bH});
        bina.setPosition({bX, ZEMIN_Y - bH});
        bina.setFillColor(sf::Color(15, 15, 25, 230));
        pencere.draw(bina);

        // Cati antenleri
        sf::RectangleShape anten({2.f, 20.f + std::rand()%30});
        anten.setPosition({bX + bW/2, ZEMIN_Y - bH - anten.getSize().y});
        anten.setFillColor(sf::Color(10, 10, 15, 220));
        pencere.draw(anten);
        // Anten tepesi kirmizi isik
        sf::CircleShape antenIsik(2.f); antenIsik.setOrigin({2.f, 2.f});
        antenIsik.setPosition({bX + bW/2 + 1.f, ZEMIN_Y - bH - anten.getSize().y});
        antenIsik.setFillColor(sf::Color(255, 30, 30, 200));
        pencere.draw(antenIsik);

        // Pencereler - gece sarı/turuncu isiklar
        for(float py = ZEMIN_Y - bH + 20; py < ZEMIN_Y - 20; py += 35) {
            for(float px = bX + 15; px < bX + bW - 20; px += 25) {
                if(std::rand()%10 > 4) continue; // Cogu pencere karanlik
                sf::RectangleShape pencereDik({12.f, 18.f});
                pencereDik.setPosition({px, py});
                int r = 200 + std::rand()%55; int g = 160 + std::rand()%60;
                pencereDik.setFillColor(sf::Color(r, g, 50, 160));
                pencere.draw(pencereDik);
            }
        }
    }
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Zemin - gece koyu toprak
    sf::VertexArray zemin(sf::PrimitiveType::TriangleStrip, 4);
    zemin[0].position = sf::Vector2f(0, ZEMIN_Y);               zemin[0].color = sf::Color(20, 30, 15);
    zemin[1].position = sf::Vector2f(PENCERE_W, ZEMIN_Y);       zemin[1].color = sf::Color(20, 30, 15);
    zemin[2].position = sf::Vector2f(0, PENCERE_H);             zemin[2].color = sf::Color(10, 15, 8);
    zemin[3].position = sf::Vector2f(PENCERE_W, PENCERE_H);     zemin[3].color = sf::Color(10, 15, 8);
    pencere.draw(zemin);

    // Zemin cizgisi
    sf::RectangleShape cizgi({PENCERE_W, 4.f});
    cizgi.setPosition({0.f, ZEMIN_Y});
    cizgi.setFillColor(sf::Color(15, 20, 10));
    pencere.draw(cizgi);
}

// =============================================================
// ARAYUZ CIZIM VE MENU YARDIMCILARI
// =============================================================
void drawButton(sf::RenderWindow& w, sf::Font& f, const std::string& text, float x, float y, float genislik, float yukseklik, bool hover) {
    sf::RectangleShape btn({genislik, yukseklik});
    btn.setPosition({x, y});
    btn.setFillColor(hover ? sf::Color(90, 90, 130) : sf::Color(40, 40, 65));
    btn.setOutlineThickness(2.f);
    btn.setOutlineColor(sf::Color(180, 180, 220));

    sf::Text txt(f, text, 26);
    txt.setFillColor(sf::Color::White);
    auto gb = txt.getGlobalBounds();
    txt.setPosition({x + genislik / 2.f - gb.size.x / 2.f, y + yukseklik / 2.f - gb.size.y / 2.f - 3.f});

    w.draw(btn);
    w.draw(txt);
}

// =============================================================
// MAIN
// =============================================================
int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({static_cast<unsigned int>(PENCERE_W), static_cast<unsigned int>(PENCERE_H)}), 
                            "Muhtesem Tank vs Tank - Gorsel Gelistirme Surumu");
    window.setFramerateLimit(60);

    SesYoneticisi sesSistemi;
    sesSys = &sesSistemi;

    sf::Font oyunFont;
    bool fontYuklendi = oyunFont.openFromFile("C:\\Windows\\Fonts\\arial.ttf");

    sf::Text skorYazi(oyunFont, "Skor: 0 | Dalga: 1", 36);
    skorYazi.setFillColor(sf::Color(255, 200, 50));
    skorYazi.setPosition({40.f, 30.f});

    std::vector<OyunNesnesi*> nesneler;
    Tank* tank = new Tank(150.f);
    nesneler.push_back(tank);

    int skor = 0;
    int dalga = 1;
    sf::Clock bulutSaati;

    while (window.isOpen()) {
        g_nesnelerEklenecek.clear();
        sf::Vector2i mPos = sf::Mouse::getPosition(window);

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            
            // Mouse & Menu kontrolleri buraya (Eski kod ile ayni mantik, kisa tutuldu)
            if (aktifDurum == OyunDurumu::MENU && event->is<sf::Event::MouseButtonPressed>()) {
                aktifDurum = OyunDurumu::OYUN_ICI;
            }
        }

        if (aktifDurum == OyunDurumu::OYUN_ICI) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) {
                if (OyunNesnesi* m = tank->atesEt()) {
                    g_nesnelerEklenecek.push_back(m);
                }
            }

            DusmanTank* dusmanObj = nullptr;
            for(OyunNesnesi* n : nesneler) {
                if(auto* a = dynamic_cast<DusmanTank*>(n)) { dusmanObj = a; break; }
            }

            if(!dusmanObj) {
                if(skor > 0 || dalga > 1) { 
                    skor += dalga * 100; dalga++;
                    skorYazi.setString("Skor: " + std::to_string(skor) + " | Dalga: " + std::to_string(dalga));
                }
                dusmanObj = new DusmanTank(PENCERE_W - 200.f, dalga);
                nesneler.push_back(dusmanObj);
            } else {
                if (OyunNesnesi* m = dusmanObj->atesEt()) {
                    g_nesnelerEklenecek.push_back(m);
                }
            }

            // Bulut olusturma
            if (bulutSaati.getElapsedTime().asSeconds() > 8.f) {
                bulutSaati.restart();
                float xPos = (std::rand() % 2 == 0) ? -300.f : PENCERE_W + 300.f;
                float yPos = 50.f + static_cast<float>(std::rand() % 200);
                float hiz = (xPos < 0) ? (1.5f + (std::rand()%10)/10.f) : (-1.5f - (std::rand()%10)/10.f);
                g_nesnelerEklenecek.push_back(new Bulut(xPos, yPos, hiz));
            }

            for (auto* nesne : nesneler) {
                nesne->guncelle();
                // Bulutlardan silah/can birakma
                if(auto* b = dynamic_cast<Bulut*>(nesne)) {
                    if(Baloncuk* kutu = b->silahBirak()) g_nesnelerEklenecek.push_back(kutu);
                }
            }

            for (auto* n1 : nesneler) {
                if (n1->silinecekMi()) continue;

                // Baloncuk (guc kutusu) carpisma kontrolu
                if (auto* b = dynamic_cast<Baloncuk*>(n1)) {
                    for (auto* n2 : nesneler) {
                        if (!n2->isMermi()) continue;
                        if (!carpistiMi(b, n2)) continue;

                        Sahip mermiSahibi = n2->getSahip();
                        GucTipi balonTipi = b->getTip();

                        if (mermiSahibi == Sahip::TANK) {
                            if (balonTipi == GucTipi::CAN) tank->canEkle();
                        } else if (mermiSahibi == Sahip::DUSMAN && dusmanObj) {
                            if (balonTipi == GucTipi::CAN) dusmanObj->canEkle();
                        }

                        b->silinmeyeIsaretle(); n2->silinmeyeIsaretle();
                        patlamaEfekti(b->getSinirlar()->position.x + 20.f, b->getSinirlar()->position.y + 20.f);
                        break;
                    }
                }

                // Mermi carpisma kontrolu
                if (n1->isMermi()) {
                    Sahip sahip = n1->getSahip();
                    if (sahip == Sahip::TANK && dusmanObj && carpistiMi(n1, dusmanObj)) {
                        dusmanObj->hasarAl();
                        patlamaEfekti(n1->getSinirlar()->position.x, n1->getSinirlar()->position.y);
                        n1->silinmeyeIsaretle();
                        skor += 5; skorYazi.setString("Skor: " + std::to_string(skor) + " | Dalga: " + std::to_string(dalga));
                    } 
                    else if (sahip == Sahip::DUSMAN && carpistiMi(n1, tank)) {
                        tank->hasarAl();
                        patlamaEfekti(n1->getSinirlar()->position.x, n1->getSinirlar()->position.y);
                        n1->silinmeyeIsaretle();
                    }
                }
            }

            for (OyunNesnesi* yeni : g_nesnelerEklenecek) nesneler.push_back(yeni);
            nesneleriTemizle(nesneler);

            if (tank->silinecekMi()) aktifDurum = OyunDurumu::OYUN_BITTI;
        }

        window.clear();
        savasMeydaniCiz(window);
        for (OyunNesnesi* n : nesneler) n->ciz(window);
        if (fontYuklendi) window.draw(skorYazi);

        // Menu gorselleri
        if (aktifDurum != OyunDurumu::OYUN_ICI) {
            sf::RectangleShape karartma({PENCERE_W, PENCERE_H});
            karartma.setFillColor(sf::Color(0, 0, 0, 185));
            window.draw(karartma);
            
            if (fontYuklendi) {
                sf::Text tBaslik(oyunFont, "TANK VS TANK", 72);
                tBaslik.setPosition({PENCERE_W/2.f - 250.f, 200.f});
                window.draw(tBaslik);
                drawButton(window, oyunFont, "TIKLA BASLA", PENCERE_W/2.f - 150.f, 400.f, 300.f, 60.f, false);
            }
        }

        window.display();
    }

    for (OyunNesnesi* n : nesneler) delete n;
    return 0;
}
