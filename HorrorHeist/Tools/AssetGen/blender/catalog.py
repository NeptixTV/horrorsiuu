"""
Item catalogue (cosmetics, equipment, jobs). The Blender scripts build the meshes and icons
listed here; the Unreal setup script turns every entry into a Primary Data Asset.
Run directly to write SourceArt/Data/catalog.json.
"""
import json
import os


def C(item_id, name, slot, mesh=None, tint=None, tint2=None, price=0, rarity='Common', level=1, owned=False,
      desc='', flavor='', theme='', bone='head', hides_hair=False, skin=False, order=0):
    return dict(id=item_id, name=name, slot=slot, mesh=mesh, tint=tint, tint2=tint2, price=price, rarity=rarity,
                level=level, owned=owned, desc=desc, flavor=flavor, theme=theme, bone=bone, hides_hair=hides_hair,
                skin=skin, order=order)


COSMETICS = [
    # Body: skin tones (tint the body material).
    C('CO_Skin_Porcelain', 'Skin Tone: Porcelain', 'Body', tint=(0.93, 0.78, 0.68), owned=True, skin=True, order=1),
    C('CO_Skin_Fair', 'Skin Tone: Fair', 'Body', tint=(0.86, 0.67, 0.54), owned=True, skin=True, order=2),
    C('CO_Skin_Olive', 'Skin Tone: Olive', 'Body', tint=(0.72, 0.54, 0.38), owned=True, skin=True, order=3),
    C('CO_Skin_Tan', 'Skin Tone: Tan', 'Body', tint=(0.60, 0.42, 0.30), owned=True, skin=True, order=4),
    C('CO_Skin_Umber', 'Skin Tone: Umber', 'Body', tint=(0.40, 0.27, 0.19), owned=True, skin=True, order=5),
    C('CO_Skin_Deep', 'Skin Tone: Deep', 'Body', tint=(0.24, 0.155, 0.11), owned=True, skin=True, order=6),

    # Hair (attached to the head).
    C('CO_Hair_Crop_Brown', 'Short Crop (Brown)', 'Hair', 'SM_HH_Hair_Crop', (0.16, 0.09, 0.05), owned=True,
      desc='Low maintenance. Fits under anything.'),
    C('CO_Hair_Crop_Black', 'Short Crop (Black)', 'Hair', 'SM_HH_Hair_Crop', (0.03, 0.025, 0.02), owned=True),
    C('CO_Hair_Messy_Blond', 'Bed Head (Sandy)', 'Hair', 'SM_HH_Hair_Messy', (0.52, 0.40, 0.22), price=400,
      desc='Slept in the van again.'),
    C('CO_Hair_Messy_Grey', 'Bed Head (Grey)', 'Hair', 'SM_HH_Hair_Messy', (0.46, 0.45, 0.43), price=700,
      rarity='Uncommon', level=4, desc='It was brown before the Marrow job.'),
    C('CO_Hair_Tied_Red', 'Tied Back (Auburn)', 'Hair', 'SM_HH_Hair_Tied', (0.33, 0.09, 0.035), price=600),
    C('CO_Hair_Tied_Black', 'Tied Back (Black)', 'Hair', 'SM_HH_Hair_Tied', (0.03, 0.025, 0.02), price=600),
    C('CO_Hair_Curly_Black', 'Curls (Black)', 'Hair', 'SM_HH_Hair_Curly', (0.035, 0.028, 0.022), price=800,
      rarity='Uncommon'),

    # Hats (hide the hair).
    C('CO_Hat_Beanie_Charcoal', 'Beanie (Charcoal)', 'Hat', 'SM_HH_Hat_Beanie', (0.12, 0.12, 0.13), price=250,
      hides_hair=True, desc='Warm, dark, forgettable.'),
    C('CO_Hat_Beanie_Mustard', 'Beanie (Mustard)', 'Hat', 'SM_HH_Hat_Beanie', (0.62, 0.43, 0.08), price=300,
      hides_hair=True, desc='Bold choice for a burglar.'),
    C('CO_Hat_FlatCap', 'Flat Cap', 'Hat', 'SM_HH_Hat_FlatCap', (0.32, 0.25, 0.17), price=650, hides_hair=True,
      rarity='Uncommon', desc="Brannigan's late father wore one."),
    C('CO_Hat_Cap_Company', 'Company Cap', 'Hat', 'SM_HH_Hat_Cap', (0.12, 0.24, 0.18), (0.72, 0.70, 0.62), price=450,
      hides_hair=True, desc='Brannigan & Sons Plumbing. Excellent cover story.'),
    C('CO_Hat_Cap_Red', 'Ball Cap (Red)', 'Hat', 'SM_HH_Hat_Cap', (0.45, 0.05, 0.04), (0.45, 0.05, 0.04), price=450,
      hides_hair=True),

    # Masks.
    C('CO_Mask_Balaclava', 'Balaclava', 'Mask', 'SM_HH_Mask_Balaclava', (0.05, 0.05, 0.055), price=500, hides_hair=True,
      desc='The classic. Itchy.'),
    C('CO_Mask_PaperBag', 'Paper Bag', 'Mask', 'SM_HH_Mask_PaperBag', None, price=300, hides_hair=True,
      desc='Two holes. Good enough.'),
    C('CO_Mask_Domino', 'Domino Mask', 'Mask', 'SM_HH_Mask_Domino', (0.05, 0.05, 0.06), price=350,
      desc='Hides almost nothing. Feels great.'),
    C('CO_Mask_Hockey', 'Goalie Mask', 'Mask', 'SM_HH_Mask_Hockey', (0.82, 0.80, 0.74), price=1200, rarity='Rare',
      level=2, desc='From the lake house job. The lake house was empty.'),
    C('CO_Mask_Gas', 'Gas Mask', 'Mask', 'SM_HH_Mask_Gas', (0.12, 0.13, 0.11), price=1800, rarity='Rare', level=3,
      desc='Breathing sounds included.'),
    C('CO_Mask_Doll', 'Porcelain Face', 'Mask', 'SM_HH_Mask_Doll', (0.86, 0.83, 0.78), (0.55, 0.12, 0.12), price=2500,
      rarity='Epic', level=5, desc='Found in the Marrow attic.', flavor='It was warm when we picked it up.'),

    # Tops (required).
    C('CO_Top_Tee_Ash', 'Plain Tee (Ash)', 'Top', 'SK_HH_Top_Tee', (0.40, 0.40, 0.40), owned=True),
    C('CO_Top_Tee_Black', 'Plain Tee (Black)', 'Top', 'SK_HH_Top_Tee', (0.04, 0.04, 0.045), owned=True),
    C('CO_Top_Tee_White', 'Plain Tee (White)', 'Top', 'SK_HH_Top_Tee', (0.80, 0.79, 0.76), price=100),
    C('CO_Top_Sweater_Stripe', 'Striped Sweater', 'Top', 'SK_HH_Top_Sweater', (0.62, 0.56, 0.44), (0.10, 0.12, 0.22),
      price=600, desc="Grandma's. Don't ask."),
    C('CO_Top_Hoodie_Grey', 'Hoodie (Grey)', 'Top', 'SK_HH_Top_Hoodie', (0.33, 0.33, 0.34), price=700),
    C('CO_Top_Hoodie_Black', 'Hoodie (Black)', 'Top', 'SK_HH_Top_Hoodie', (0.04, 0.04, 0.045), price=700),
    C('CO_Top_Hoodie_Forest', 'Hoodie (Forest)', 'Top', 'SK_HH_Top_Hoodie', (0.08, 0.18, 0.12), price=750),
    C('CO_Top_Turtleneck', 'Turtleneck', 'Top', 'SK_HH_Top_Turtleneck', (0.03, 0.03, 0.035), price=900,
      rarity='Uncommon', desc='Cat burglar classic.'),

    # Jackets.
    C('CO_Jacket_Work', 'Work Jacket', 'Jacket', 'SK_HH_Jacket_Work', (0.42, 0.33, 0.19), price=800,
      desc='Canvas, pockets, plausible deniability.'),
    C('CO_Jacket_Bomber', 'Bomber Jacket', 'Jacket', 'SK_HH_Jacket_Bomber', (0.17, 0.20, 0.12), price=1200),
    C('CO_Jacket_Rain', 'Rain Jacket', 'Jacket', 'SK_HH_Jacket_Rain', (0.78, 0.56, 0.04), price=1100, rarity='Uncommon',
      level=2, desc='Yellow. Like hers.'),
    C('CO_Jacket_Leather', 'Leather Jacket', 'Jacket', 'SK_HH_Jacket_Leather', (0.06, 0.055, 0.05), price=1600,
      rarity='Rare', level=3),
    C('CO_Jacket_Vest', 'Tactical Vest', 'Jacket', 'SK_HH_Jacket_Vest', (0.10, 0.11, 0.08), price=2000, rarity='Rare',
      level=4, desc='Bought online. Nobody asked questions.'),

    # Gloves.
    C('CO_Gloves_Work', 'Work Gloves', 'Gloves', 'SK_HH_Gloves', (0.42, 0.28, 0.14), price=200),
    C('CO_Gloves_Black', 'Leather Gloves', 'Gloves', 'SK_HH_Gloves', (0.04, 0.035, 0.03), price=250),
    C('CO_Gloves_Latex', 'Latex Gloves', 'Gloves', 'SK_HH_Gloves_Thin', (0.35, 0.47, 0.62), price=150,
      desc='No prints, no excuses.'),

    # Pants (required).
    C('CO_Pants_Jeans', 'Jeans', 'Pants', 'SK_HH_Pants_Jeans', (0.24, 0.32, 0.50), owned=True),
    C('CO_Pants_Jeans_Black', 'Black Jeans', 'Pants', 'SK_HH_Pants_Jeans', (0.08, 0.08, 0.09), owned=True),
    C('CO_Pants_Cargo_Olive', 'Cargo Pants (Olive)', 'Pants', 'SK_HH_Pants_Cargo', (0.22, 0.24, 0.14), price=600),
    C('CO_Pants_Cargo_Black', 'Cargo Pants (Black)', 'Pants', 'SK_HH_Pants_Cargo', (0.05, 0.05, 0.05), price=600),
    C('CO_Pants_Slacks', 'Slacks', 'Pants', 'SK_HH_Pants_Slacks', (0.20, 0.20, 0.22), price=500),

    # Shoes (required).
    C('CO_Shoes_Sneakers_White', 'Sneakers (White)', 'Shoes', 'SK_HH_Shoes_Sneakers', (0.76, 0.75, 0.72),
      (0.80, 0.78, 0.72), owned=True),
    C('CO_Shoes_Sneakers_Black', 'Sneakers (Black)', 'Shoes', 'SK_HH_Shoes_Sneakers', (0.05, 0.05, 0.05),
      (0.75, 0.73, 0.68), owned=True),
    C('CO_Shoes_Boots', 'Work Boots', 'Shoes', 'SK_HH_Shoes_Boots', (0.30, 0.18, 0.09), price=700),
    C('CO_Shoes_Dress', 'Dress Shoes', 'Shoes', 'SK_HH_Shoes_Dress', (0.03, 0.025, 0.02), price=650,
      desc='Quiet soles. Mostly.'),

    # Backpacks (attached to spine_03).
    C('CO_Back_Rucksack', 'Rucksack', 'Backpack', 'SM_HH_Back_Rucksack', (0.10, 0.13, 0.22), price=500,
      bone='spine_03'),
    C('CO_Back_Duffel', 'Shoulder Duffel', 'Backpack', 'SM_HH_Back_Duffel', (0.05, 0.05, 0.05), price=900,
      bone='spine_03'),
    C('CO_Back_Tactical', 'Tactical Pack', 'Backpack', 'SM_HH_Back_Tactical', (0.17, 0.19, 0.11), price=1400,
      rarity='Rare', level=3, bone='spine_03'),

    # Accessories.
    C('CO_Acc_Glasses', 'Reading Glasses', 'Accessory', 'SM_HH_Acc_Glasses', (0.10, 0.07, 0.04), price=250),
    C('CO_Acc_Watch', 'Wristwatch', 'Accessory', 'SM_HH_Acc_Watch', (0.75, 0.6, 0.3), price=300, bone='lowerarm_l',
      desc='Always 3:17 for some reason.'),
    C('CO_Acc_Earpiece', 'Earpiece', 'Accessory', 'SM_HH_Acc_Earpiece', (0.05, 0.05, 0.05), price=400),
    C('CO_Acc_Chain', 'Gold Chain', 'Accessory', 'SM_HH_Acc_Chain', (0.78, 0.6, 0.28), price=750, rarity='Uncommon',
      bone='spine_03'),
]


def stat(stat_id, label, value, display_max=100.0, lower_better=False):
    return dict(id=stat_id, label=label, value=value, max=display_max, lower_better=lower_better)


def E(item_id, name, slot, mesh, stats, price=0, rarity='Common', level=1, owned=False, desc='', flavor='', order=0,
      shop=''):
    """shop: where it is sold - '' the Fence in the hideout, 'Hardware' Cobb's Hardware, 'Market' Pick & Save."""
    return dict(id=item_id, name=name, slot=slot, mesh=mesh, stats=stats, price=price, rarity=rarity, level=level,
                owned=owned, desc=desc, flavor=flavor, order=order, shop=shop)


def light_stats(intensity, rng, cone, warmth):
    return [stat('Intensity', 'Brightness', intensity, 3000), stat('Range', 'Reach', rng, 3000),
            stat('Cone', 'Beam Width', cone, 80), stat('Warmth', 'Warmth', warmth, 1.0)]


EQUIPMENT = [
    E('EQ_Light_Pocket', 'Pocket Torch', 'Light', 'SM_EQ_Torch_Pocket', light_stats(900, 1400, 22, 0.75), owned=True,
      desc='Cheap, warm and just bright enough.', order=1),
    E('EQ_Light_Patrol', 'Patrol Torch', 'Light', 'SM_EQ_Torch_Patrol', light_stats(2600, 2600, 16, 0.3), price=900,
      desc='Long, heavy, very bright. Also a club.', order=2, shop='Hardware'),
    E('EQ_Light_Headlamp', 'Headlamp', 'Light', 'SM_EQ_Headlamp', light_stats(1100, 1500, 34, 0.5), price=1200,
      rarity='Uncommon', desc='Hands free. Looks where you look.', order=3, shop='Hardware'),
    E('EQ_Light_Lantern', 'Storm Lantern', 'Light', 'SM_EQ_Lantern', light_stats(700, 1000, 70, 1.0), price=1500,
      rarity='Rare', level=2, desc='Soft light all around. Like the painting.', order=4, shop='Hardware'),

    E('EQ_Entry_Lockpicks', 'Lockpick Roll', 'Entry', 'SM_EQ_Lockpicks',
      [stat('Speed', 'Speed', 40), stat('Noise', 'Noise', 12, lower_better=True)], owned=True,
      desc='Slow and silent.', order=1),
    E('EQ_Entry_Crowbar', 'Crowbar', 'Entry', 'SM_EQ_Crowbar',
      [stat('Speed', 'Speed', 85), stat('Noise', 'Noise', 80, lower_better=True)], price=400,
      desc='Fast and loud. Everyone will know.', order=2, shop='Hardware'),
    E('EQ_Entry_GlassCutter', 'Glass Cutter', 'Entry', 'SM_EQ_GlassCutter',
      [stat('Speed', 'Speed', 60), stat('Noise', 'Noise', 25, lower_better=True)], price=1100, rarity='Uncommon',
      desc='Windows are just doors that nobody locks properly.', order=3),

    E('EQ_Util_Walkie', 'Walkie-Talkie', 'Utility', 'SM_EQ_Walkie',
      [stat('Range', 'Range', 60), stat('Battery', 'Battery', 70)], owned=True,
      desc='Use it to mark a spot for the whole crew (they hear the click and see where).', order=1),
    E('EQ_Util_Salt', 'Salt Pouch', 'Utility', 'SM_EQ_SaltPouch',
      [stat('Charges', 'Charges', 3, 5), stat('Strength', 'Strength', 45)], price=250,
      desc='Use it to pour a line on the floor. Whatever walks the house stops at it - once.', order=2,
      shop='Market'),
    E('EQ_Util_FirstAid', 'First Aid Kit', 'Utility', 'SM_EQ_FirstAid',
      [stat('Speed', 'Revive Speed', 80), stat('Charges', 'Uses', 2, 5)], price=350,
      desc='Gets a downed crewmate up in two seconds instead of five - and steadier. Two uses.', order=3,
      shop='Market'),

    E('EQ_Gadget_Camera', 'Instant Camera', 'Gadget', 'SM_EQ_Camera',
      [stat('Flash', 'Flash', 60), stat('Shots', 'Film', 10, 12)], price=800,
      desc='Use it to take a flash photo. The flash drives it off for a moment; every photo of it pays $150.',
      order=1, shop='Market'),
    E('EQ_Gadget_NoiseMeter', 'Noise Meter', 'Gadget', 'SM_EQ_NoiseMeter',
      [stat('Sensitivity', 'Sensitivity', 70), stat('Range', 'Range', 50)], price=1300, rarity='Rare', level=2,
      desc='Hold it: the needle jumps when something is close - and pins itself just before it hunts.', order=2),

    E('EQ_Bag_Duffel', 'Duffel Bag', 'Bag', 'SM_EQ_Duffel',
      [stat('Capacity', 'Capacity', 50), stat('Noise', 'Rustle', 20, lower_better=True)], owned=True, order=1,
      desc='Heavy loot slows you down less the more the bag holds.'),
    E('EQ_Bag_Sack', 'Burlap Sack', 'Bag', 'SM_EQ_Sack',
      [stat('Capacity', 'Capacity', 75), stat('Noise', 'Rustle', 45, lower_better=True)], price=500,
      desc='Holds more: heavy loot slows you down less. Clinks louder when you run.', order=2, shop='Hardware'),
]

MISSIONS = [
    dict(id='JOB_Vance', name='The Vance Residence', address='14 Ashgrove Lane', difficulty='Quiet', residents=3,
         crew=2, pay=(1800, 3200), level=1, photo='T_Mission_Vance', order=1,
         target='Coin collection (master bedroom)',
         briefing='Suburban two-storey. Cash in the study, a coin collection upstairs. The back door sticks. '
                  'The Vances never seem to leave the house, not even to sleep.',
         rumor='The neighbours say the youngest Vance stopped going to school years ago. Nobody remembers '
               'there being a youngest Vance.'),
    dict(id='JOB_Rectory', name='St. Agnes Rectory', address='Church Road', difficulty='Uneasy', residents=1, crew=2,
         pay=(3500, 6000), level=2, photo='T_Mission_Rectory', order=2, target='The parish silver',
         briefing='Father Doyle keeps the collection money in the vestry safe until Monday. He is old, half deaf, '
                  'and talks to someone in the empty church at night.',
         rumor='The bell rings at 3:17 every night. There is no rope.'),
    dict(id='JOB_Marrow', name='The Marrow House', address='Marrow Street', difficulty='Restless', residents=0, crew=3,
         pay=(6000, 9500), level=3, photo='T_Mission_Marrow', order=3, target="Josiah Marrow's study for "
         '"The Lantern Bearer"',
         briefing='Empty since 1974. The family left everything behind. A buyer wants the study sketch for the '
                  "museum's famous painting; it should still hang upstairs.",
         rumor="Every October there's a light in the attic window. The power was cut in '75."),
    dict(id='JOB_Whitlock', name='Whitlock Farmhouse', address='Old Quarry Road', difficulty='Malevolent', residents=2,
         crew=4, pay=(12000, 20000), level=5, photo='T_Mission_Whitlock', order=4, target='Whatever is in the barn',
         briefing="Brannigan won't talk about the Whitlocks. Someone pays very well for what is locked in the barn. "
                  'Nobody who went out there came back to say what it is.',
         rumor='The red light in the barn has been on since 1998.'),
]

def I(id_, kind, title, detail, value=(0, 0), known=False, how='', key=''):
    """One piece of job intel (FHHJobIntel). Ids are unique across all jobs."""
    return dict(id=id_, kind=kind, title=title, detail=detail, value=list(value), known=known, how=how, key=key)


MAILBOX = 'Check the Vances\' mailbox by the lane.'
WATCH = 'Watch the house by day from the bench across Ashgrove Lane.'
# What the crew knows about each job (dossier). Unknown entries are learned by scouting in town
# (HHIntelSpot actors placed by vance.py). Keys found by day are in the crew's hands on the job.
INTEL = {
    'JOB_Vance': [
        I('Vance_Coins', 'Valuable', 'Coin collection (the target)',
          'Master bedroom - upstairs, front left. On the dresser, under glass.', (1200, 2200), known=True),
        I('Vance_StudyCash', 'Valuable', 'Cash in the study',
          'Study - ground floor, front right. Desk drawer: search the desk.', (300, 700), known=True),
        I('Vance_LivingRoom', 'Valuable', 'Living room electronics and the mantel',
          'Living room - ground floor, front left. Flat-screen TV (heavy, slows you down), a game console, '
          'a mantel clock and a silver candlestick.', (700, 1400),
          how='Peek through the living-room window by day (front yard, left of the porch).'),
        I('Vance_StudyDesk', 'Valuable', 'Laptop and an old camera',
          'Study - a laptop on the desk, a vintage camera on the floor by the bookcase.', (370, 750),
          how='Peek through the study window by day (front yard, right of the porch).'),
        I('Vance_Pearls', 'Valuable', "Mrs Vance's pearls and his gold watch",
          'Master bedroom nightstand. She wears the pearls at the window every day.', (750, 1700), how=WATCH),
        I('Vance_Ring', 'Valuable', 'Diamond ring',
          'An insurance letter says she keeps it "in the bathroom cabinet" - upstairs, back right.', (500, 1200),
          how=MAILBOX),
        I('Vance_Locket', 'Valuable', 'Old locket in the basement trunk',
          'Basement, in the old trunk. Worth a lot. Whatever lives in the house wants it to stay there.', (600, 1400),
          how='Look through the basement window at the back of the house by day.'),
        I('Vance_Harold', 'Owner', 'Harold Vance',
          'Sixties, retired bank clerk. Nobody has seen him outside in years; his car has not moved since spring.',
          how=MAILBOX),
        I('Vance_Margaret', 'Owner', 'Margaret Vance',
          'Sits at the upstairs window all day. She does not read, does not knit. She watches the lane.', how=WATCH),
        I('Vance_Youngest', 'Owner', '"The youngest"',
          'Letters for a Thomas Vance, aged 9, postmarked 1987. Still delivered. There is no Thomas in the phone book.',
          how=MAILBOX),
        I('Vance_FrontDoor', 'Entry', 'Front door',
          'Old pin lock. Lockpicks get you in in about seven seconds.', known=True),
        I('Vance_BackDoor', 'Entry', 'Back door (kitchen)',
          'Sticks. A crowbar opens it, loudly. The glass panel can be cut quietly.', known=True),
        I('Vance_SpareKey', 'Entry', 'Spare key under the garden gnome',
          'Taken by day: the crew walks in through the front door at night, no picking.',
          how='Look under the garden gnome by the porch.', key='VanceSpareKey'),
        I('Vance_Neighbour', 'Security', 'Mr Pruitt next door',
          'Sleeps lightly. If he hears something he comes out with a torch to look - and calls the police if he '
          'sees anyone. Stay out of his light, or be gone before he gets there.', known=True),
        I('Vance_Patrol', 'Security', 'Night patrol',
          'A patrol car drives down Ashgrove Lane some nights. Do not be standing on the lawn when it passes.',
          known=True),
        I('Vance_NoAlarm', 'Security', 'No alarm, no dog',
          'No alarm box on the wall, no dog bowl. The upstairs lights go out around eleven.', how=WATCH),
        I('Vance_Basement', 'Note', 'Basement - stay out of it',
          'The crew who cased it before you wrote this on the plan. The house gets angrier down there.', known=True),
    ],
    'JOB_Rectory': [
        I('Rectory_Safe', 'Valuable', 'Collection money', 'Vestry safe, until Monday.', (2000, 3500), known=True),
        I('Rectory_Doyle', 'Owner', 'Father Doyle', 'Old, half deaf. Talks to someone in the empty church at night.',
          known=True),
    ],
    'JOB_Marrow': [
        I('Marrow_Study', 'Valuable', 'The study sketch', 'Upstairs study - "The Lantern Bearer" sketch.',
          (3000, 5000), known=True),
        I('Marrow_Empty', 'Owner', 'Nobody', 'Empty since 1974. The family left everything behind.', known=True),
    ],
}
FLOOR_PLANS = {'JOB_Vance': 'T_Blueprint_Vance'}


# Tool tags: which lock / interaction an item counts as when it sits in the 5-slot inventory on a job.
TOOL_TAGS = {
    'Light': 'Light', 'EQ_Entry_Lockpicks': 'Lockpick', 'EQ_Entry_Crowbar': 'Crowbar',
    'EQ_Entry_GlassCutter': 'GlassCutter', 'EQ_Util_Walkie': 'Walkie', 'EQ_Util_Salt': 'Salt',
    'EQ_Util_FirstAid': 'FirstAid',
    'EQ_Gadget_Camera': 'Camera', 'EQ_Gadget_NoiseMeter': 'NoiseMeter', 'Bag': 'Bag',
}
for _e in EQUIPMENT:
    _e['tool'] = TOOL_TAGS.get(_e['id'], TOOL_TAGS.get(_e['slot'], ''))

# Job maps and rules (the Vance job is released; Marrow and the rectory are prepared but closed).
MAPS = '/Game/HorrorHeist/Maps/'
for _m in MISSIONS:
    _m.update({
        'JOB_Vance': dict(map=MAPS + 'L_Job_Vance', available=True, night=True, min_haul=1500,
                          target_item='LT_CoinCollection', aggression=1.0, house='HH_House_Vance'),
        'JOB_Rectory': dict(map=MAPS + 'L_Job_Rectory', available=False, night=True, min_haul=3000,
                            target_item='', aggression=1.3, house='HH_House_Rectory'),
        'JOB_Marrow': dict(map=MAPS + 'L_Job_Marrow', available=False, night=True, min_haul=5000,
                           target_item='', aggression=1.6, house='HH_House_Marrow'),
    }.get(_m['id'], dict(map='', available=False, night=True, min_haul=8000, target_item='', aggression=2.0,
                         house='')))
    _m['intel'] = INTEL.get(_m['id'], [])
    _m['floor_plan'] = FLOOR_PLANS.get(_m['id'], '')


def L(id_, name, mesh, lo, hi, anger=2.0, heavy=False, cursed=False, desc='', rarity='Common'):
    return dict(id=id_, name=name, mesh=mesh, value=(lo, hi), anger=anger, heavy=heavy, cursed=cursed, desc=desc,
                rarity=rarity, sound='S_Loot_Pickup_Heavy_01' if heavy else 'S_Loot_Pickup_Small_0%d' % (1 + len(name) % 3))


# Things worth stealing. Values are what the fence pays; 'anger' is how much taking it upsets the house.
LOOT = [
    L('LT_TV', 'Flat-Screen TV', 'SM_Loot_TV', 300, 500, 4.0, heavy=True, desc='Heavy. Carry it with both hands.'),
    L('LT_Laptop', 'Laptop', 'SM_Loot_Laptop', 250, 450, 2.0),
    L('LT_JewelryBox', 'Jewellery Box', 'SM_Loot_JewelryBox', 400, 900, 4.0, rarity='Uncommon'),
    L('LT_Watch', 'Gold Wristwatch', 'SM_Loot_Watch', 350, 800, 3.0, rarity='Uncommon'),
    L('LT_Ring', 'Diamond Ring', 'SM_Loot_Ring', 500, 1200, 4.0, rarity='Rare'),
    L('LT_Necklace', 'Pearl Necklace', 'SM_Loot_Necklace', 400, 900, 3.0, rarity='Uncommon'),
    L('LT_Cash', 'Cash', 'SM_Loot_Cash', 300, 700, 2.0),
    L('LT_Silverware', 'Silverware Case', 'SM_Loot_Silverware', 300, 600, 3.0),
    L('LT_Vase', 'Porcelain Vase', 'SM_Loot_Vase', 150, 400, 2.0),
    L('LT_Console', 'Game Console', 'SM_Loot_Console', 150, 300, 1.0),
    L('LT_Tablet', 'Tablet', 'SM_Loot_Tablet', 120, 250, 1.0),
    L('LT_Phone', 'Phone', 'SM_Loot_Phone', 80, 200, 1.0),
    L('LT_Perfume', 'Perfume', 'SM_Loot_Perfume', 60, 150, 1.0),
    L('LT_Whiskey', 'Aged Whiskey', 'SM_Loot_Whiskey', 80, 180, 1.0),
    L('LT_Painting', 'Small Oil Painting', 'SM_Loot_Painting', 250, 700, 4.0, rarity='Uncommon'),
    L('LT_Camera', 'Vintage Camera', 'SM_Loot_Camera', 120, 300, 2.0),
    L('LT_Candlestick', 'Silver Candlestick', 'SM_Loot_Candlestick', 100, 250, 3.0),
    L('LT_MusicBox', 'Music Box', 'SM_Loot_MusicBox', 150, 350, 8.0, desc='It was playing when you found it.',
      rarity='Uncommon'),
    L('LT_CoinJar', 'Coin Jar', 'SM_Loot_CoinJar', 40, 110, 1.0),
    L('LT_MantelClock', 'Mantel Clock', 'SM_Loot_MantelClock', 150, 350, 3.0),
    L('LT_Figurine', 'Porcelain Figurine', 'SM_Loot_Figurine', 80, 220, 3.0),
    L('LT_Handbag', 'Designer Handbag', 'SM_Loot_Handbag', 200, 500, 2.0),
    L('LT_Locket', 'Old Locket', 'SM_Loot_Locket', 600, 1400, 6.0, cursed=True,
      desc='A portrait inside. You feel watched while you carry it.', rarity='Rare'),
    L('LT_CoinCollection', 'Coin Collection', 'SM_Loot_CoinCollection', 1200, 2200, 10.0,
      desc="The client's target. Twenty old coins under glass.", rarity='Rare'),
]

DEFAULTS = dict(
    cosmetics=dict(Body='CO_Skin_Fair', Hair='CO_Hair_Crop_Brown', Top='CO_Top_Tee_Ash', Pants='CO_Pants_Jeans',
                   Shoes='CO_Shoes_Sneakers_White'),
    equipment=dict(Light='EQ_Light_Pocket', Entry='EQ_Entry_Lockpicks', Utility='EQ_Util_Walkie', Bag='EQ_Bag_Duffel'),
    starting_cash=1500,
)


def write(path=None):
    here = os.path.dirname(os.path.abspath(__file__))
    path = path or os.path.join(here, '..', '..', '..', 'SourceArt', 'Data', 'catalog.json')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as fh:
        json.dump(dict(cosmetics=COSMETICS, equipment=EQUIPMENT, missions=MISSIONS, loot=LOOT, defaults=DEFAULTS), fh,
                  indent=1)
    return path


if __name__ == '__main__':
    print(write())
