from enum import Enum

class Intent(Enum):
    ATTRACT_ATTENTION_INTENT = "AttractAttentionIntent" 
    ENGAGEMENT_INTENT = "EngagementIntent"   # kid noticed a robot / reacts / talks
    PLAY_INTENT = "PlayIntent"               # kid wants to play
    DISTRESS_INTENT = "DistressIntent"       # kid is upset 
    FALLBACK_INTENT = "FallbackIntent"
    
    def to_state(self):
        from interaction.state import State
        mapping = {
            Intent.ATTRACT_ATTENTION_INTENT: State.ATTRACTING,
            Intent.ENGAGEMENT_INTENT: State.ENGAGED,
            Intent.PLAY_INTENT: State.PLAYING,
            Intent.DISTRESS_INTENT: State.CALMING,
            Intent.FALLBACK_INTENT: State.ERROR
        }
        return mapping.get(self, State.ERROR)