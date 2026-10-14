// All-in-One Command Menu for Arma Reforger
// Based on Leopard20's All-In-One Command Menu for Arma 3 (GPLv3); rebuilt in Enforce Script.
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Squad formations: the game's own four (SCR_AIWorld's formation list: Wedge, Line, Column,
// StaggeredColumn). Arma 3's echelons, vee and diamond would need new entries in that list,
// which is native prefab data (see CLAUDE.md, Formations).

class AIO_Formations
{
	static bool IsFormationOrder(int order)
	{
		return order >= AIO_EOrder.FORM_COLUMN && order <= AIO_EOrder.FORM_LINE;
	}

	static SCR_EAIGroupFormation FormationOf(int order)
	{
		switch (order)
		{
			case AIO_EOrder.FORM_COLUMN: return SCR_EAIGroupFormation.Column;
			case AIO_EOrder.FORM_STAGGERED_COLUMN: return SCR_EAIGroupFormation.StaggeredColumn;
			case AIO_EOrder.FORM_LINE: return SCR_EAIGroupFormation.Line;
		}
		return SCR_EAIGroupFormation.Wedge;
	}

	static string LabelOf(SCR_EAIGroupFormation formation)
	{
		switch (formation)
		{
			case SCR_EAIGroupFormation.Column: return "column";
			case SCR_EAIGroupFormation.StaggeredColumn: return "staggered column";
			case SCR_EAIGroupFormation.Line: return "line";
		}
		return "wedge";
	}

	static string NameOf(SCR_EAIGroupFormation formation)
	{
		return typename.EnumToString(SCR_EAIGroupFormation, formation);
	}

	// Is the formation in the world's list? (A scenario could ship its own AI world.)
	static bool Exists(SCR_EAIGroupFormation formation)
	{
		AIWorld world = GetGame().GetAIWorld();
		return world && world.GetFormation(NameOf(formation));
	}

	// Server: the squad's formation, kept as a commanding setting on the group so the game's
	// own move activities re-apply it, and applied at once to the formation in use now.
	static string Set(SCR_AIGroup group, int order)
	{
		SCR_EAIGroupFormation formation = FormationOf(order);
		string label = LabelOf(formation);
		if (!Exists(formation))
		{
			Print(string.Format("[AIO] formation %1 is not in the AI world's formation list", NameOf(formation)), LogLevel.WARNING);
			return "Formation " + label + " is not available in this scenario";
		}
		SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(group.FindComponent(SCR_AIGroupSettingsComponent));
		if (settings)
			settings.AddSetting(SCR_AIGroupFormationSetting.Create(SCR_EAISettingOrigin.COMMANDING, formation), false, true);
		bool applied = Apply(group, formation);
		Print(string.Format("[AIO] formation %1 for %2 (setting %3, applied %4)", NameOf(formation), group, settings != null, applied));
		if (!applied)
			return "Formation " + label + " could not be set";
		return "Formation: " + label;
	}

	// Same as the game's SCR_AISetGroupFormation node: formation component and every move handler.
	static bool Apply(SCR_AIGroup group, SCR_EAIGroupFormation formation)
	{
		string name = NameOf(formation);
		bool ok;
		AIFormationComponent formationComponent = AIFormationComponent.Cast(group.FindComponent(AIFormationComponent));
		if (formationComponent)
			ok = formationComponent.SetFormation(name);
		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(group.FindComponent(AIGroupMovementComponent));
		if (movement)
		{
			int handler = 0;
			while (movement.GetMoveHandlerAgentCount(handler) != -1)
			{
				if (movement.SetFormationDefinition(handler, name))
					ok = true;
				handler++;
			}
		}
		return ok;
	}

	// The formation the squad's main move handler uses now ("" if none).
	static string Current(SCR_AIGroup group)
	{
		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(group.FindComponent(AIGroupMovementComponent));
		if (movement && movement.GetFormationDefinition(0))
			return movement.GetFormationDefinition(0).GetName();
		AIFormationComponent formationComponent = AIFormationComponent.Cast(group.FindComponent(AIFormationComponent));
		if (formationComponent && formationComponent.GetFormation())
			return formationComponent.GetFormation().GetName();
		return "";
	}
}
