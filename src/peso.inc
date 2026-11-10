// peso.inc -- [peso] peso de la mochila, armero y comienzo con solo el cuchillo. Se incluye desde game.c.
// Aquí vive casi todo; en game.c solo hay ganchos cortos marcados con // [peso].

#define PESO_BASE 30.0f      // kg que aguanta la mochila de partida
#define PESO_DEF 0.3f        // lo que pesa un objeto que no está en la tabla
#define PESO_FLAG_GUN 1      // ya se tomó la Makarov del cajón de la primera expedición

static WeaponState g_arm[W_MAX];   // el armero: armas guardadas (owned = 1 si hay una)
static int g_peso_flags;           // PESO_FLAG_*
static int g_arm_sel, g_arm_top;

// Otro agente la usará para un artefacto que da carga: kg extra de capacidad
static float peso_extra_kg(void) { return artef_kg(); }   // la Semilla hueca (artef.inc) suma 15 kg

// kg por unidad de cada objeto; lo que no está aquí pesa PESO_DEF (la chatarra, las mochilas y el arma tirada no pesan)
static const float PESO_ITEM[ITEM_MAX] = {
    [ITEM_BAND] = 0.05f, [ITEM_MED] = 0.3f, [ITEM_FOOD] = 0.4f, [ITEM_WATER] = 0.5f, [ITEM_ARAD] = 0.2f, [ITEM_FILTER] = 0.3f,
    [ITEM_CLEAN] = 0.3f, [ITEM_SEW] = 0.3f, [ITEM_RAG] = 0.1f, [ITEM_CHEM] = 0.2f, [ITEM_MECH] = 0.4f, [ITEM_POWDER] = 0.2f,
    [ITEM_9MM] = 0.012f, [ITEM_9MM_AP] = 0.012f, [ITEM_9MM_HP] = 0.012f,
    [ITEM_762] = 0.016f, [ITEM_762_AP] = 0.016f, [ITEM_762_HP] = 0.016f,
    [ITEM_12G] = 0.045f, [ITEM_12G_SLUG] = 0.045f,
    [ITEM_SIL] = 0.4f, [ITEM_COMP] = 0.3f, [ITEM_XMAG] = 0.3f, [ITEM_REFLEX] = 0.3f, [ITEM_STOCK] = 0.8f,
    [ITEM_JACKET] = 2.5f, [ITEM_VEST] = 3.0f, [ITEM_HVEST] = 6.0f, [ITEM_MASK] = 0.8f, [ITEM_DET] = 0.6f,
    [ITEM_NUT] = 0.05f, [ITEM_GREN] = 0.4f, [ITEM_MOLO] = 0.6f, [ITEM_MINE] = 1.2f, [ITEM_TRAP] = 1.5f,
    [ITEM_BK_BOX] = 2.0f,
    [ITEM_BKD_LAMP] = 1.0f, [ITEM_BKD_RUG] = 1.5f, [ITEM_BKD_SHELF] = 3.0f, [ITEM_BKD_POSTER] = 0.1f, [ITEM_BKD_PLANT] = 0.8f,
    [ITEM_BKD_RADIO] = 1.5f, [ITEM_BKD_TABLE] = 4.0f, [ITEM_BKD_CHAIR] = 2.0f,
    [ITEM_LONGB] = 0.8f, [ITEM_BRAKE] = 0.2f, [ITEM_SCOPE] = 0.6f, [ITEM_DRUM] = 0.6f, [ITEM_PADSTOCK] = 0.5f,
    [ITEM_LIGHT] = 0.3f, [ITEM_LASER] = 0.2f, [ITEM_BIPOD] = 0.8f, [ITEM_BAYO] = 0.4f, [ITEM_REPAIR] = 0.5f,
    // [duro] suministros y objetos de valor: lo pesado es lo que más conviene traer, para que haya que elegir
    [ITEM_D_HERR] = 1.5f, [ITEM_D_COBRE] = 0.8f, [ITEM_D_ELEC] = 0.4f, [ITEM_D_BATERIA] = 0.7f, [ITEM_D_CINTA] = 0.1f,
    [ITEM_D_ANTIB] = 0.05f, [ITEM_D_ALCOHOL] = 0.5f, [ITEM_D_CAFE] = 0.2f, [ITEM_D_TABACO] = 0.05f, [ITEM_D_MAPA] = 0.05f,
    [ITEM_D_COMBUS] = 3.0f, [ITEM_D_FILTRO] = 0.5f,
    // [artef] artefactos y piezas únicas
    [ITEM_ART0] = 0.5f, [ITEM_ART0 + 1] = 0.5f, [ITEM_ART0 + 2] = 0.5f, [ITEM_ART0 + 3] = 0.5f, [ITEM_ART0 + 4] = 0.5f, [ITEM_ART0 + 5] = 0.5f,
    [ITEM_ART6] = 0.5f, [ITEM_ART7] = 0.5f, [ITEM_ART8] = 0.5f, [ITEM_ART9] = 0.5f,
};

static float peso_item_kg(int i)
{
    if (i < 0 || i >= ITEM_COUNT) return 0;
    if (i == ITEM_SCRAP || i == ITEM_PACK1 || i == ITEM_PACK2 || i == ITEM_WDROP) return 0;
    return PESO_ITEM[i] > 0 ? PESO_ITEM[i] : PESO_DEF;
}

static float peso_weapon_kg(int w)
{
    switch (w) {
    case W_KNIFE: return 0.3f;
    case W_MAKAROV: return 0.8f;
    case W_REVOLVER: return 1.0f;
    case W_SMG: return 2.5f;
    case W_SAWED: return 2.2f;
    case W_PUMP: return 3.2f;
    case W_AK: return 3.6f;
    case W_BOLT: return 4.0f;
    case W_VIBORA: return 0.9f;   // [armas2]
    case W_CHACAL: return 1.3f;
    case W_ZUMBA: return 2.3f;
    case W_TORM: return 3.0f;
    case W_CUERVO: return 3.4f;
    case W_HALCON: return 3.8f;
    case W_CENTINELA: return 4.6f;
    case W_APLANA: return 7.5f;
    case W_AXE: return 1.5f;
    case W_PIPE: return 1.8f;
    case W_MAUL: return 5.0f;
    }
    return 1.0f;
}

// Cuánto aguanta: base + Mula + la mejor mochila que tengas (no se suman) + lo que dé un artefacto
static float peso_cap(void)
{
    float c = PESO_BASE + (g_sk[SK_MULA] ? 10.0f : 0) + peso_extra_kg();
    if (g_bag[ITEM_PACK2] > 0) c += 18.0f;
    else if (g_bag[ITEM_PACK1] > 0) c += 8.0f;
    return c;
}

// Lo que cargas: mochila, armas llevadas y equipo puesto (el alijo, las cajas y el armero no pesan)
static float peso_total(void)
{
    float t = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (g_bag[i] > 0) t += g_bag[i] * peso_item_kg(i);
    for (int w = 0; w < W_COUNT; w++) if (g_w[w].owned) t += peso_weapon_kg(w);
    for (int k = 0; k < EQ_COUNT; k++) if (g_eq[k] > 0) t += peso_item_kg(GEAR[g_eq[k] - 1].item);
    return t;
}

static float peso_pct(void) { return peso_total() * 100.0f / peso_cap(); }

// Cuántos más caben de un objeto: manda el peso. Sobre 140 % no entra nada
static int peso_room(int item)
{
    float kg = peso_item_kg(item);
    if (item == ITEM_PACK1 || item == ITEM_PACK2) return g_bag[item] > 0 ? 0 : 1;   // una de cada una
    if (kg <= 0) return 1 << 20;
    float free_kg = peso_cap() * 1.4f - peso_total();
    if (free_kg <= 0) return 0;
    int n = (int)(free_kg / kg);
    return n > 1 << 20 ? 1 << 20 : n;
}

// Aviso cuando algo no entra por el peso
static void peso_warn(int item, int room)
{
    if (room <= 0 && peso_item_kg(item) > 0 && peso_pct() >= 140)
        msg("Vas demasiado cargado: no puedes recoger nada más.");
}

static void peso_fmt(char *s, int n, float v) { snprintf(s, n, "%.1f", v); for (char *p = s; *p; p++) if (*p == '.') *p = ','; }

static u32 peso_col(float pct) { return pct > 140 ? RGBA(240, 90, 80, 255) : pct > 100 ? RGBA(240, 90, 80, 255) : pct > 80 ? RGBA(236, 200, 90, 255) : RGBA(130, 210, 110, 255); }

// En la mochila: "Carga 23,4/30 kg" abajo a la derecha (arriba a la izquierda va el trabajo) y el aviso a su derecha   // [qsist]
static void peso_draw_bag(void)
{
    char a[16], b[16], s[64];
    float pct = peso_pct();
    peso_fmt(a, sizeof(a), peso_total()); peso_fmt(b, sizeof(b), peso_cap());
    snprintf(s, sizeof(s), "Carga %s/%s kg", a, b);
    r_text(272, 218, s, peso_col(pct));
    if (pct > 100) text_right(458, 218, pct > 140 ? "Lleno" : "No corres", peso_col(pct));
}

// En el HUD: un aviso chico, sobre la barra de vida (no encima de ella)   // [qsist]
static void peso_hud(void)
{
    float pct = peso_pct();
    if (pct <= 100) return;
    char s[32];
    snprintf(s, sizeof(s), "CARGA %d %%", (int)pct);
    r_text_shadow(8, 218, s, peso_col(pct));
}

// Velocidad: sobre 100 %, 30 % más lento
static float peso_slow(void) { return peso_pct() > 100 ? 0.7f : 1.0f; }

// ---------------------------------------------------------------- armero
static int peso_carried(void) { int n = 0; for (int i = 0; i < W_COUNT; i++) n += g_w[i].owned; return n; }

static void peso_arm_toggle(int w)
{
    char s[96];
    if (g_w[w].owned) {   // llevada: se guarda
        if (peso_carried() <= 1) { msg("Siempre debes llevar al menos un arma."); return; }
        g_arm[w] = g_w[w];
        memset(&g_w[w], 0, sizeof(g_w[w]));
        if (P.weapon == w) { for (int i = 0; i < W_COUNT; i++) if (g_w[i].owned) { P.weapon = i; break; } P.jammed = 0; P.reload_t = 0; }
        snprintf(s, sizeof(s), "Guardaste: %s.", WDEF[w].name);
        msg(s);
    } else if (g_arm[w].owned) {   // guardada: se toma
        g_w[w] = g_arm[w];
        memset(&g_arm[w], 0, sizeof(g_arm[w]));
        snprintf(s, sizeof(s), "Tomaste: %s.", WDEF[w].name);
        msg(s);
    }
}

static int peso_arm_list(unsigned char *out)
{
    int n = 0;
    for (int i = 0; i < W_COUNT; i++) if (g_w[i].owned || g_arm[i].owned) out[n++] = (unsigned char)i;
    return n;
}

#define ARM_ROWS 7
static void peso_arm_input(void)
{
    unsigned char list[W_MAX];
    int n = peso_arm_list(list);
    list_keep(&g_arm_sel, &g_arm_top, n, 1, ARM_ROWS);
    if (!n) return;
    if (in.pressed & PSP_CTRL_UP) g_arm_sel = (g_arm_sel + n - 1) % n;
    if (in.pressed & PSP_CTRL_DOWN) g_arm_sel = (g_arm_sel + 1) % n;
    list_keep(&g_arm_sel, &g_arm_top, n, 1, ARM_ROWS);
    if (in.pressed & PSP_CTRL_CROSS) { peso_arm_toggle(list[g_arm_sel]); n = peso_arm_list(list); list_keep(&g_arm_sel, &g_arm_top, n, 1, ARM_ROWS); }
}

static void peso_arm_draw(void)
{
    char s[96], a[16], b[16];
    unsigned char list[W_MAX];
    int n = peso_arm_list(list), sel = g_arm_sel, top = g_arm_top;
    list_keep(&sel, &top, n, 1, ARM_ROWS);
    ui_panel(14, 60, 452, 168, "ARMERO");
    peso_fmt(a, sizeof(a), peso_total()); peso_fmt(b, sizeof(b), peso_cap());
    snprintf(s, sizeof(s), "Carga %s / %s kg", a, b);
    text_right(458, 62, s, peso_col(peso_pct()));
    r_text(80, 62, "Cruz: llevar / guardar", RGBA(190, 200, 150, 255));
    for (int r = 0; r < ARM_ROWS && top + r < n; r++) {
        int i = top + r, w = list[i], on = i == sel, carried = g_w[w].owned;
        const WeaponState *ws = carried ? &g_w[w] : &g_arm[w];
        float y = 80 + r * 20;
        Spr ic = SPR[WDEF[w].spr];
        ui_row(22, y, 436, 18, on);
        r_spr(ic, 38 - ic.w / 2, y + 9 - ic.h / 2, COL_WHITE);
        r_text(58, y + 3, WDEF[w].name, on ? COL_WHITE : UI_LIGHT);
        snprintf(s, sizeof(s), "%s", carried ? "LLEVADA" : "guardada");
        r_text(180, y + 3, s, carried ? RGBA(130, 210, 110, 255) : UI_DIM);
        snprintf(s, sizeof(s), "estado %d %%", (int)(ws->cond * 100 + 0.5f));
        r_text(260, y + 3, s, ws->cond < 0.5f ? RGBA(240, 150, 90, 255) : UI_LIGHT);
        int np = 0;
        for (int k = 0; k < SLOT_COUNT; k++) np += ws->part[k] != 0;
        snprintf(s, sizeof(s), "piezas %d", np);
        r_text(350, y + 3, s, np ? UI_LIGHT : UI_DIM);
        peso_fmt(b, sizeof(b), peso_weapon_kg(w));
        snprintf(s, sizeof(s), "%s kg", b);
        text_right(450, y + 3, s, UI_DIM);
    }
    ui_scroll(461, 80, 138, top, ARM_ROWS, n);
}

// ---------------------------------------------------------------- comienzo con el cuchillo
static void peso_new_game(void)
{
    memset(g_w, 0, sizeof(g_w));
    memset(g_arm, 0, sizeof(g_arm));
    g_peso_flags = 0;
    g_w[W_KNIFE] = (WeaponState){1, 0, 0.95f};
    P.weapon = W_KNIFE;
    memset(g_bag, 0, sizeof(g_bag));
    g_bag[ITEM_BAND] = 1; g_bag[ITEM_FOOD] = 1; g_bag[ITEM_WATER] = 1;
}

static void peso_save(WeaponState *arm, int *flags) { memcpy(arm, g_arm, sizeof(g_arm)); *flags = g_peso_flags; }

static void peso_load(const WeaponState *arm, int flags)
{
    memcpy(g_arm, arm, sizeof(g_arm));
    for (int i = 0; i < W_MAX; i++) {
        g_arm[i].owned = g_arm[i].owned != 0 && i < W_COUNT && !g_w[i].owned;
        if (!g_arm[i].owned) { memset(&g_arm[i], 0, sizeof(g_arm[i])); continue; }
        g_arm[i].cond = clampf(g_arm[i].cond, 0.05f, 1);
        if (g_arm[i].load < 0 || g_arm[i].load >= WDEF[i].nammo) g_arm[i].load = 0;
        if (g_arm[i].mag < 0) g_arm[i].mag = 0;
    }
    g_peso_flags = flags;
    if (!peso_carried()) g_w[W_KNIFE].owned = 1;   // siempre llevas al menos una
}

// El arma de partida si la elegida ya no se lleva
static int peso_first_weapon(void) { for (int i = 0; i < W_COUNT; i++) if (g_w[i].owned) return i; return W_KNIFE; }

// Las armas halladas vienen gastadas: 30 a 60 %
static void peso_found_cond(int w) { g_w[w].cond = 0.3f + 0.3f * frand(); }

// ---------------------------------------------------------------- cajón del arma de la primera expedición
static int peso_first_exped(void) { return g_xtotal == 1 && !(g_peso_flags & PESO_FLAG_GUN); }

// Se abre el cajón del arma: en la primera expedición es la Makarov gastada con 10 balas. Devuelve 1 si lo atendió
static int peso_crate_gun(float x, float y)
{
    if (!peso_first_exped()) return 0;
    g_peso_flags |= PESO_FLAG_GUN;
    memset(&g_arm[W_MAKAROV], 0, sizeof(g_arm[W_MAKAROV]));
    g_w[W_MAKAROV] = (WeaponState){1, 0, 0.45f};
    g_w[W_MAKAROV].mag = 0;
    {
        int take = inv_add(ITEM_9MM, 10);
        if (take < 10) spill(ITEM_9MM, 10 - take);
    }
    pop(x, y, "Pistola Makarov", RGBA(150, 220, 130, 255));
    msg("Encontraste una Makarov gastada y 10 balas. Cambia de arma con izquierda o derecha.");
    return 1;
}

// Al salir a la primera expedición: avisa del cajón del arma
static void peso_exped_hint(void)
{
    if (peso_first_exped()) msg("Hay un cajón con un arma lejos de la entrada: míralo en el mapa (SELECT).");
}

// Marca en el mapa de SELECT el cajón del arma de la primera expedición
static void peso_map_marks(float x0, float y0)
{
    if (!peso_first_exped() || g_zone != 1) return;
    for (int i = 0; i < g_ncr; i++)
        if (g_cr[i].kind == CK_WEAPON && !g_cr[i].open)
            map_mark(x0 + g_cr[i].tx * MAP_PX + MAP_PX / 2, y0 + g_cr[i].ty * MAP_PX + MAP_PX / 2, 4, RGBA(240, 80, 70, 255));
}
